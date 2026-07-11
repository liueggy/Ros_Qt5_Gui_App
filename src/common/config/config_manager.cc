#include "config/config_manager.h"
#include <QFile>
#include <boost/dll.hpp>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

#include "boost/filesystem.hpp"
namespace Config {
bool ConfigManager::writeStringToFile(const std::string &filePath,
                                      const std::string &content) {
  boost::filesystem::path path(filePath);
  if (!boost::filesystem::exists(path.parent_path())) {
    boost::filesystem::create_directories(path.parent_path());  // 创建路径
  }

  std::ofstream outputFile(filePath);
  if (outputFile) {
    outputFile << content;
    outputFile.flush();
    const bool success = outputFile.good();
    outputFile.close();
    return success && !outputFile.fail();
  } else {
    std::cerr << "无法创建文件 " << filePath << std::endl;
    return false;
  }
}

// #define CHECK_DEFALUT
ConfigManager::ConfigManager(/* args */) {
  Init(config_path_);
}
void ConfigManager::Init(const std::string &config_path) {
  config_path_ = config_path;
  // 配置不存在 写入默认配置
  if (!boost::filesystem::exists(config_path_)) {
    nlohmann::json j = config_root_;
    std::string pretty_json = j.dump(2);

    writeStringToFile(config_path_, pretty_json);
  }
  ReadRootConfig();
}
ConfigManager::~ConfigManager() {
  std::lock_guard<std::mutex> lock(mutex_);
  nlohmann::json j = config_root_;
  std::string pretty_json = j.dump(2);
  std::cout << "write json" << std::endl;
  writeStringToFile(config_path_, pretty_json);
}
bool ConfigManager::ReadRootConfig() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ifstream file(config_path_);
  try {
    if (!file.is_open()) {
      throw std::runtime_error("cannot open configuration file");
    }
    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    ConfigRoot parsed;
    std::string parse_error;
    if (!ParseRootConfig(content, parsed, &parse_error)) {
      throw std::runtime_error(parse_error);
    }
    config_root_ = std::move(parsed);
  } catch (const std::exception &e) {
    fprintf(stderr, "Error parsing config.json error: %s\n", e.what());
    file.close();

    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    const boost::filesystem::path original(config_path_);
    const boost::filesystem::path backup(config_path_ + ".corrupt." +
                                         std::to_string(timestamp));
    boost::system::error_code backup_error;
    if (boost::filesystem::exists(original)) {
      boost::filesystem::rename(original, backup, backup_error);
      if (backup_error) {
        fprintf(stderr, "Unable to preserve invalid config: %s\n",
                backup_error.message().c_str());
      }
    }

    config_root_ = ConfigRoot{};
    if (!StoreConfigUnlocked()) {
      fprintf(stderr, "Unable to write default configuration to %s\n",
              config_path_.c_str());
    }
    return false;
  }
  file.close();

  return true;
}
bool ConfigManager::StoreConfig() {
  std::lock_guard<std::mutex> lock(mutex_);
  return StoreConfigUnlocked();
}

bool ConfigManager::StoreConfigUnlocked() {
  nlohmann::json j = config_root_;
  std::string pretty_json = j.dump(2);
  return writeStringToFile(config_path_, pretty_json);
}

bool ConfigManager::ParseRootConfig(const std::string &content,
                                    ConfigRoot &config, std::string *error) {
  try {
    ConfigRoot parsed = nlohmann::json::parse(content).get<ConfigRoot>();
    config = std::move(parsed);
    return true;
  } catch (const std::exception &exception) {
    if (error) {
      *error = exception.what();
    }
    return false;
  }
}
std::string ConfigManager::GetTopicName(const std::string &frame_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(config_root_.display_config.begin(),
                           config_root_.display_config.end(),
                           [&frame_name](const auto &item) {
                             return item.display_name == frame_name;
                           });
  if (iter == config_root_.display_config.end()) {
    return "";
  }
  return iter->topic;
}

ConfigRoot ConfigManager::GetRootConfigSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_root_;
}

bool ConfigManager::UpdateRootConfig(
    const std::function<void(ConfigRoot&)> &update, bool persist) {
  std::lock_guard<std::mutex> lock(mutex_);
  update(config_root_);
  return !persist || StoreConfigUnlocked();
}

void ConfigManager::SetDefaultTopicName(const std::string &frame_name,
                                        const std::string &topic_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(config_root_.display_config.begin(),
                           config_root_.display_config.end(),
                           [&frame_name](const auto &item) {
                             return item.display_name == frame_name;
                           });
  if (iter == config_root_.display_config.end()) {
    config_root_.display_config.push_back(
        DisplayConfig(frame_name, topic_name, true));
  } else if (iter->topic.empty()) {
    iter->topic = topic_name;
  }
  StoreConfigUnlocked();
}

std::string ConfigManager::GetConfigValue(const std::string &key, const std::string &default_value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = config_root_.key_value.find(key);
  if (it != config_root_.key_value.end()) {
    return it->second;
  }
  return default_value;
}

void ConfigManager::SetConfigValue(const std::string &key, const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_root_.key_value[key] = value;
  StoreConfigUnlocked();
}

void ConfigManager::SetDefaultKeyValue(const std::string &key, const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (config_root_.key_value.find(key) == config_root_.key_value.end()) {
    config_root_.key_value[key] = value;
    StoreConfigUnlocked();
  }
}

bool ConfigManager::ReadTopologyMap(const std::string &map_path,
                                    TopologyMap &map) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string fullPath = map_path;
  boost::filesystem::path inputPathObj(map_path);
  // 判断路径是否为相对路径
  if (inputPathObj.is_relative()) {
    // 获取当前可执行程序的路径
    boost::filesystem::path executablePath = boost::dll::program_location();

    // 使用 absolute 函数将相对路径转换为绝对路径
    boost::filesystem::path absolutePath = boost::filesystem::absolute(inputPathObj, executablePath.parent_path());

    // 将路径转换为字符串
    fullPath = absolutePath.string();
  }

  std::ifstream file(fullPath);
  try {
    nlohmann::json j;
    file >> j;
    map = j.get<TopologyMap>();
  } catch (const std::exception &e) {
    fprintf(stderr, "Error parsing struct %s\n", e.what());
    file.close();
    return false;
  }
  file.close();
  return true;
}

bool ConfigManager::WriteTopologyMap(const std::string &map_path,
                                     const TopologyMap &topology_map) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string fullPath = map_path;
  boost::filesystem::path inputPathObj(map_path);
  // 判断路径是否为相对路径
  if (inputPathObj.is_relative()) {
    // 获取当前可执行程序的路径
    boost::filesystem::path executablePath = boost::dll::program_location();

    // 使用 absolute 函数将相对路径转换为绝对路径
    boost::filesystem::path absolutePath = boost::filesystem::absolute(inputPathObj, executablePath.parent_path());

    // 将路径转换为字符串
    fullPath = absolutePath.string();
  }
  nlohmann::json j = topology_map;
  std::string pretty_json = j.dump(2);
  return writeStringToFile(fullPath, pretty_json);
}

}  // namespace Config
