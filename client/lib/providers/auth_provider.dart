import 'package:flutter/foundation.dart';
import '../services/api_service.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AuthProvider with ChangeNotifier {
  final ApiService _apiService = ApiService();

  String? _token;
  String? _username;
  String? _userId;
  Map<String, dynamic>? _currentUser;
  bool _isLoading = false;
  String? _error;

  AuthProvider() {
    _loadToken();
  }

  Future<void> _loadToken() async {
    final prefs = await SharedPreferences.getInstance();
    _token = prefs.getString('auth_token');
    _username = prefs.getString('username');
    _userId = prefs.getString('user_id');
    print('[AuthProvider] Loaded userId: $_userId');
    notifyListeners();
  }

  Future<void> _saveToken(String token, String username, String userId) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('auth_token', token);
    await prefs.setString('username', username);
    await prefs.setString('user_id', userId);
    print('[AuthProvider] Saved userId: $userId');
  }

  Future<void> _clearToken() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('auth_token');
    await prefs.remove('username');
    await prefs.remove('user_id');
  }

  // Getters
  String? get token => _token;
  String? get username => _username;
  String? get userId => _userId;
  Map<String, dynamic>? get currentUser => _currentUser;
  bool get isLoading => _isLoading;
  String? get error => _error;
  bool get isAuthenticated => _token != null;

  // Clear error
  void clearError() {
    _error = null;
    notifyListeners();
  }

  // Register user
  Future<bool> register(String username, String email, String password) async {
    _isLoading = true;
    _error = null;
    notifyListeners();
    try {
      final response = await _apiService.register(username, email, password);
      print('Register response: $response');

      if (response['success'] == true && response['token'] != null) {
        // New server format: token is directly in response
        _token = response['token'];
        _username = username;
        _userId = response['token']; // Token is the user ID in new format

        _currentUser = {'id': _userId, 'username': username, 'email': email};

        await _saveToken(_token!, _username!, _userId!);
        _isLoading = false;
        _error = null;
        notifyListeners();
        return true;
      } else {
        _error =
            response['error'] ?? response['message'] ?? 'Registration failed';
        _isLoading = false;
        notifyListeners();
        return false;
      }
    } catch (e) {
      _error = 'Network error: $e';
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  // Login user
  Future<bool> login(String username, String password) async {
    print('[AuthProvider] Starting login for username: $username');
    _isLoading = true;
    _error = null;
    notifyListeners();
    try {
      final response = await _apiService.login(username, password);
      print('[AuthProvider] Login response: $response');

      if (response['success'] == true && response['token'] != null) {
        print('[AuthProvider] Login successful, setting user data');

        // New server format: token is directly in response
        _token = response['token'];
        _username = username;
        _userId = response['token']; // Token is the user ID in new format

        _currentUser = {
          'id': _userId,
          'username': username,
          'email': '', // Email not returned in login response
        };

        await _saveToken(_token!, _username!, _userId!);
        _isLoading = false;
        _error = null;
        notifyListeners();
        print('[AuthProvider] Login completed successfully');
        return true;
      } else {
        _error =
            response['error']?.toString() ??
            response['message']?.toString() ??
            'Login failed';
        _isLoading = false;
        notifyListeners();
        print('[AuthProvider] Login failed: $_error');
        return false;
      }
    } catch (e) {
      _error = 'Network error: $e';
      _isLoading = false;
      notifyListeners();
      print('[AuthProvider] Login exception: $e');
      return false;
    }
  }

  // Logout user
  void logout() {
    _token = null;
    _username = null;
    _userId = null;
    _currentUser = null;
    _error = null;
    _clearToken();
    notifyListeners();
  }

  // Get user profile
  Future<Map<String, dynamic>?> getProfile() async {
    if (_userId == null || _token == null) return null;

    try {
      final profile = await _apiService.getProfile(_userId!, _token!);
      if (profile['success'] == true && profile['user'] != null) {
        _currentUser = profile['user'];
        notifyListeners();
        return profile;
      } else {
        _error = profile['error'] ?? 'Failed to get profile';
        notifyListeners();
        return null;
      }
    } catch (e) {
      _error = e.toString();
      notifyListeners();
      return null;
    }
  }
}
