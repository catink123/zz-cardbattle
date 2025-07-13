import 'dart:convert';
import 'package:http/http.dart' as http;
import 'server_config.dart';

class ApiService {
  // Auth endpoints
  static const String registerEndpoint = '/auth/register';
  static const String loginEndpoint = '/auth/login';
  static const String profileEndpoint = '/auth/profile';

  // Session endpoints (HTTP for session management only)
  static const String createSessionEndpoint = '/game/create-session';
  static const String joinSessionEndpoint = '/game/join-session';
  static const String leaveSessionEndpoint = '/game/leave-session';
  static const String getSessionsEndpoint = '/game/sessions';

  // Health endpoint
  static const String healthEndpoint = '/health';

  Future<Map<String, dynamic>> makeRequest(
    String endpoint, {
    String method = 'GET',
    Map<String, dynamic>? body,
    Map<String, String>? headers,
    String? token,
  }) async {
    final ip = await ServerConfig.getServerIp();
    final uri = Uri.parse('http://$ip:8080$endpoint');
    final requestHeaders = {
      'Content-Type': 'application/json',
      if (token != null) 'Authorization': 'Bearer $token',
      ...?headers,
    };

    print('[ApiService] Making $method request to: $uri');
    print('[ApiService] Headers: $requestHeaders');
    if (body != null) {
      print('[ApiService] Body: $body');
    }

    http.Response response;

    try {
      switch (method.toUpperCase()) {
        case 'GET':
          response = await http.get(uri, headers: requestHeaders);
          break;
        case 'POST':
          response = await http.post(
            uri,
            headers: requestHeaders,
            body: body != null ? jsonEncode(body) : null,
          );
          break;
        case 'PUT':
          response = await http.put(
            uri,
            headers: requestHeaders,
            body: body != null ? jsonEncode(body) : null,
          );
          break;
        case 'DELETE':
          response = await http.delete(uri, headers: requestHeaders);
          break;
        default:
          throw Exception('Unsupported HTTP method: $method');
      }

      print('[ApiService] Response status: ${response.statusCode}');
      print('[ApiService] Response body: ${response.body}');

      if (response.statusCode >= 200 && response.statusCode < 300) {
        try {
          if (response.body.isEmpty) {
            print('[ApiService] Empty response body');
            return {'success': true, 'message': 'Empty response'};
          }
          final decoded = jsonDecode(response.body);
          print('[ApiService] Decoded response: $decoded');
          return decoded;
        } catch (e) {
          print('[ApiService] JSON decode error: $e');
          print('[ApiService] Response body was: "${response.body}"');
          throw Exception('Invalid JSON response: $e');
        }
      } else {
        print(
          '[ApiService] Error response: HTTP ${response.statusCode}: ${response.body}',
        );
        throw Exception('HTTP ${response.statusCode}: ${response.body}');
      }
    } catch (e) {
      print('[ApiService] Exception caught: $e');
      throw Exception('Network error: $e');
    }
  }

  // Auth methods
  Future<Map<String, dynamic>> register(
    String username,
    String email,
    String password,
  ) async {
    return await makeRequest(
      registerEndpoint,
      method: 'POST',
      body: {'username': username, 'email': email, 'password': password},
    );
  }

  Future<Map<String, dynamic>> login(String username, String password) async {
    return await makeRequest(
      loginEndpoint,
      method: 'POST',
      body: {'username': username, 'password': password},
    );
  }

  Future<Map<String, dynamic>> getProfile(String userId, String token) async {
    return await makeRequest('$profileEndpoint?user_id=$userId', token: token);
  }

  // Session management methods (HTTP only)
  Future<Map<String, dynamic>> createSession(String token) async {
    return await makeRequest(
      createSessionEndpoint,
      method: 'POST',
      token: token,
    );
  }

  Future<Map<String, dynamic>> joinSession(
    String token,
    String sessionId,
  ) async {
    return await makeRequest(
      joinSessionEndpoint,
      method: 'POST',
      token: token,
      body: {'session_id': sessionId},
    );
  }

  Future<Map<String, dynamic>> leaveSession(
    String token,
    String sessionId,
  ) async {
    return await makeRequest(
      leaveSessionEndpoint,
      method: 'POST',
      token: token,
      body: {'session_id': sessionId},
    );
  }

  Future<Map<String, dynamic>> getSessions(String token) async {
    return await makeRequest(getSessionsEndpoint, token: token);
  }

  // Health check
  Future<Map<String, dynamic>> healthCheck() async {
    return await makeRequest(healthEndpoint);
  }
}
