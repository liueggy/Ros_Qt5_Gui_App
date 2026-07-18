#include "client/socket_websocket_connection.h"
#include "protocol_validation.h"

namespace {
long long SteadyMillisecondsNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
constexpr auto kHeartbeatInterval = std::chrono::seconds(5);
constexpr auto kReceiveDeadline = std::chrono::seconds(15);
}  // namespace

namespace rosbridge2cpp {

bool SocketWebSocketConnection::Init(std::string p_ip_addr, int p_port) {
  std::lock_guard<std::mutex> shutdown_lock(shutdown_mutex_);
  shutting_down_ = false;
  terminate_receiver_thread_ = false;
  is_connected_ = false;
  last_receive_ms_ = 0;
  heartbeat_error_reported_ = false;
  receive_overload_reported_ = false;
  inbound_payloads_.Reset();
  ip_addr_ = p_ip_addr;
  port_ = p_port;

  // Construct WebSocket URI
  uri_ = "ws://" + ip_addr_ + ":" + std::to_string(port_);

  std::cout << "[WebSocketConnection] Initializing connection to " << uri_ << std::endl;

  try {
    // Set logging to be pretty verbose (everything except message payloads)
    c_.set_access_channels(websocketpp::log::alevel::all);
    c_.clear_access_channels(websocketpp::log::alevel::frame_payload);
    c_.set_error_channels(websocketpp::log::elevel::all);

    // Initialize ASIO
    c_.init_asio();

    // Register our message handler
    c_.set_message_handler(bind(&SocketWebSocketConnection::on_message, this, ::_1, ::_2));
    c_.set_open_handler(bind(&SocketWebSocketConnection::on_open, this, ::_1));
    c_.set_close_handler(bind(&SocketWebSocketConnection::on_close, this, ::_1));
    c_.set_fail_handler(bind(&SocketWebSocketConnection::on_fail, this, ::_1));
    c_.set_pong_handler(bind(&SocketWebSocketConnection::on_pong, this, ::_1, ::_2));

    // Create a connection to the given URI and queue it for connection once
    // the event loop starts
    websocketpp::lib::error_code ec;
    client::connection_ptr con = c_.get_connection(uri_, ec);
    if (ec) {
      std::cout << "[WebSocketConnection] Could not create connection because: " << ec.message() << std::endl;
      return false;
    }

    hdl_ = con->get_handle();
    c_.connect(con);

    // Start the ASIO io_service run loop
    asio_thread_ = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &c_);

    // Wait for connection to be established
    std::unique_lock<std::mutex> lock(connection_mutex_);
    if (!connection_cv_.wait_for(lock, std::chrono::seconds(2),
                                 [this] { return is_connected_.load() || shutting_down_.load(); }) ||
        !is_connected_) {
      std::cout << "[WebSocketConnection] Connection timeout" << std::endl;
      lock.unlock();
      shutting_down_ = true;
      c_.stop();
      if (asio_thread_ && asio_thread_->joinable()) asio_thread_->join();
      asio_thread_.reset();
      return false;
    }

    std::cout << "[WebSocketConnection] Connected successfully" << std::endl;

    // Keep JSON parsing and ROS callbacks away from the ASIO socket thread so
    // outgoing control frames can be flushed while telemetry is processed.
    dispatch_thread_ =
        std::thread(&SocketWebSocketConnection::DispatchThreadFunction, this);

    // Setting up the receiver thread
    std::cout << "[WebSocketConnection] Setting up receiver thread..." << std::endl;
    receiver_thread_ = std::thread([=]() {ReceiverThreadFunction(); return 1; });

    return true;

  } catch (websocketpp::exception const& e) {
    std::cout << "[WebSocketConnection] Exception: " << e.what() << std::endl;
    return false;
  }
}

bool SocketWebSocketConnection::SendMessage(std::string data) {
  if (!is_connected_) {
    std::cout << "[WebSocketConnection] Not connected, cannot send message" << std::endl;
    return false;
  }

  try {
    websocketpp::lib::error_code ec;
    c_.send(hdl_, data, websocketpp::frame::opcode::text, ec);
    if (ec) {
      std::cout << "[WebSocketConnection] Send failed: " << ec.message() << std::endl;
      return false;
    }
    return true;
  } catch (websocketpp::exception const& e) {
    std::cout << "[WebSocketConnection] Send exception: " << e.what() << std::endl;
    return false;
  }
}

int SocketWebSocketConnection::ReceiverThreadFunction() {
  std::cout << "[WebSocketConnection] Receiver thread started" << std::endl;

  auto next_ping = std::chrono::steady_clock::now();
  while (!terminate_receiver_thread_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto now = std::chrono::steady_clock::now();
    if (!is_connected_ || now < next_ping) continue;
    next_ping = now + kHeartbeatInterval;
    const long long last = last_receive_ms_.load();
    if (last > 0 &&
        !validation::IsReceiveFresh(std::chrono::milliseconds(last),
                                    std::chrono::milliseconds(SteadyMillisecondsNow()),
                                    kReceiveDeadline)) {
      is_connected_ = false;
      if (!heartbeat_error_reported_.exchange(true)) {
        ReportError(TransportError::R2C_HEARTBEAT_TIMEOUT);
      }
      break;
    }
    websocketpp::lib::error_code ec;
    c_.ping(hdl_, "qt-rosbridge-heartbeat", ec);
    if (ec) {
      is_connected_ = false;
      ReportError(TransportError::R2C_SOCKET_ERROR);
      break;
    }
  }

  std::cout << "[WebSocketConnection] Receiver thread terminated" << std::endl;
  return 0;
}

void SocketWebSocketConnection::DispatchThreadFunction() {
  std::string payload;
  while (inbound_payloads_.WaitPop(&payload)) {
    if (shutting_down_) break;

    json document;
    document.Parse(payload.c_str(), payload.size());
    if (document.HasParseError()) {
      std::cout << "[WebSocketConnection] JSON parse error - Ignoring message"
                << std::endl;
      continue;
    }

    std::string validation_error;
    if (!validation::ValidateEnvelope(document, &validation_error)) {
      std::cout << "[WebSocketConnection] Invalid rosbridge envelope: "
                << validation_error << std::endl;
      continue;
    }

    std::function<void(json&)> callback;
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      callback = incoming_message_callback_;
    }
    if (callback && !shutting_down_) callback(document);
  }
}

bool SocketWebSocketConnection::IsHealthy() const {
  const long long last = last_receive_ms_.load();
  return is_connected_.load() && last > 0 &&
         validation::IsReceiveFresh(std::chrono::milliseconds(last),
                                    std::chrono::milliseconds(SteadyMillisecondsNow()),
                                    kReceiveDeadline);
}

void SocketWebSocketConnection::RegisterIncomingMessageCallback(std::function<void(json&)> fun) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  incoming_message_callback_ = fun;
  callback_function_defined_ = true;
}

void SocketWebSocketConnection::RegisterErrorCallback(std::function<void(TransportError)> fun) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  error_callback_ = fun;
}

void SocketWebSocketConnection::ReportError(TransportError err) {
  std::function<void(TransportError)> callback;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback = error_callback_;
  }
  if (callback && !shutting_down_) callback(err);
}

void SocketWebSocketConnection::SetTransportMode(ITransportLayer::TransportMode mode) {
  // Only JSON mode is supported
}

void SocketWebSocketConnection::Disconnect() {
  std::lock_guard<std::mutex> shutdown_lock(shutdown_mutex_);
  if (shutting_down_.exchange(true)) return;
  {
    std::lock_guard<std::mutex> callback_lock(callback_mutex_);
    incoming_message_callback_ = nullptr;
    error_callback_ = nullptr;
    callback_function_defined_ = false;
  }
  inbound_payloads_.Close();
  if (is_connected_) {
    try {
      websocketpp::lib::error_code ec;
      c_.close(hdl_, websocketpp::close::status::normal, "", ec);
      if (ec) {
        std::cout << "[WebSocketConnection] Error on close: " << ec.message() << std::endl;
      }
    } catch (websocketpp::exception const& e) {
      std::cout << "[WebSocketConnection] Exception on close: " << e.what() << std::endl;
    }
    is_connected_ = false;
  }

  terminate_receiver_thread_ = true;
  connection_cv_.notify_all();
  c_.stop();
  if (asio_thread_ && asio_thread_->joinable()) asio_thread_->join();
  asio_thread_.reset();
  if (receiver_thread_.joinable()) receiver_thread_.join();
  if (dispatch_thread_.joinable()) dispatch_thread_.join();
}

void SocketWebSocketConnection::on_open(connection_hdl hdl) {
  if (shutting_down_) return;
  std::cout << "[WebSocketConnection] Connection opened" << std::endl;
  std::unique_lock<std::mutex> lock(connection_mutex_);
  is_connected_ = true;
  last_receive_ms_ = SteadyMillisecondsNow();
  heartbeat_error_reported_ = false;
  connection_cv_.notify_all();
}

void SocketWebSocketConnection::on_close(connection_hdl hdl) {
  std::cout << "[WebSocketConnection] Connection closed" << std::endl;
  is_connected_ = false;
  if (!shutting_down_) {
    ReportError(TransportError::R2C_CONNECTION_CLOSED);
  }
}

void SocketWebSocketConnection::on_fail(connection_hdl hdl) {
  std::cout << "[WebSocketConnection] Connection failed" << std::endl;
  is_connected_ = false;
  if (!shutting_down_) {
    ReportError(TransportError::R2C_SOCKET_ERROR);
  }
  std::unique_lock<std::mutex> lock(connection_mutex_);
  connection_cv_.notify_all();
}

void SocketWebSocketConnection::on_message(connection_hdl hdl, message_ptr msg) {
  if (shutting_down_) return;
  const std::string& payload = msg->get_payload();
  if (payload.size() > validation::kMaxEnvelopeBytes) {
    std::cout << "[WebSocketConnection] Oversized JSON envelope - Ignoring message" << std::endl;
    return;
  }
  last_receive_ms_ = SteadyMillisecondsNow();
  if (!inbound_payloads_.Push(payload) &&
      !receive_overload_reported_.exchange(true)) {
    std::cout << "[WebSocketConnection] Inbound queue overloaded; reconnecting"
              << std::endl;
    is_connected_ = false;
    ReportError(TransportError::R2C_SOCKET_ERROR);
  }
}

bool SocketWebSocketConnection::on_pong(connection_hdl, std::string) {
  if (shutting_down_) return false;
  last_receive_ms_ = SteadyMillisecondsNow();
  return true;
}
}  // namespace rosbridge2cpp
