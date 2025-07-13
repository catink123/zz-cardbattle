import 'package:shared_preferences/shared_preferences.dart';

class ServerConfig {
  static Future<String> getServerIp() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString('server_ip') ?? 'localhost';
  }
}
