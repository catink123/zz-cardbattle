import 'dart:convert';
import 'dart:async';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:web_socket_channel/status.dart' as status;
import 'server_config.dart';

class WebSocketService {
  WebSocketChannel? _channel;
  StreamController<Map<String, dynamic>>? _messageController;
  bool _isConnected = false;
  String? _sessionId;
  String? _userId;

  // Getters
  bool get isConnected => _isConnected;
  Stream<Map<String, dynamic>>? get messageStream => _messageController?.stream;

  // Connect to battle WebSocket
  Future<bool> connectToBattle(String sessionId, String userId) async {
    try {
      print('[WebSocketService] Starting connection...');
      _sessionId = sessionId;
      _userId = userId;

      print(
        '[WebSocketService] Connecting to WebSocket with sessionId: $sessionId, userId: $userId',
      );

      // Fetch server IP
      final ip = await ServerConfig.getServerIp();
      // Create WebSocket connection to the new battle endpoint
      final uri = Uri.parse(
        'ws://$ip:8080/battle/ws?session_id=$sessionId&user_id=$userId',
      );
      print('[WebSocketService] WebSocket URI: $uri');

      _channel = WebSocketChannel.connect(uri);
      print('[WebSocketService] WebSocket channel created');

      // Create message controller
      _messageController = StreamController<Map<String, dynamic>>.broadcast();
      print('[WebSocketService] Message controller created');

      // Listen for messages
      _channel!.stream.listen(
        (message) {
          print('[WebSocketService] Raw WebSocket message received: $message');
          try {
            final data = jsonDecode(message);
            print('[WebSocketService] Parsed WebSocket message: $data');
            _messageController?.add(data);
          } catch (e) {
            print('[WebSocketService] Error parsing WebSocket message: $e');
          }
        },
        onError: (error) {
          print('[WebSocketService] WebSocket error: $error');
          _isConnected = false;
          _messageController?.addError(error);
        },
        onDone: () {
          print('[WebSocketService] WebSocket connection closed');
          _isConnected = false;
          _messageController?.close();
        },
      );

      print('[WebSocketService] WebSocket stream listener set up');
      _isConnected = true;
      print(
        '[WebSocketService] WebSocket connected to battle session: $sessionId',
      );
      return true;
    } catch (e) {
      print('[WebSocketService] Error connecting to WebSocket: $e');
      _isConnected = false;
      return false;
    }
  }

  // Send a message
  void sendMessage(Map<String, dynamic> message) {
    print('[WebSocketService] sendMessage called with: $message');
    print(
      '[WebSocketService] _isConnected: $_isConnected, _channel: ${_channel != null}',
    );

    if (_isConnected && _channel != null) {
      try {
        final jsonMessage = jsonEncode(message);
        print('[WebSocketService] Sending WebSocket message: $jsonMessage');
        _channel!.sink.add(jsonMessage);
        print('[WebSocketService] Message sent successfully');
      } catch (e) {
        print('[WebSocketService] Error sending WebSocket message: $e');
      }
    } else {
      print('[WebSocketService] WebSocket not connected - cannot send message');
    }
  }

  // Send play card action
  void playCard(int handIndex) {
    sendMessage({
      'action': 'play_card',
      'session_id': _sessionId,
      'user_id': _userId,
      'hand_index': handIndex,
    });
  }

  // Send attack action
  void attack(int attackerIndex, int targetIndex) {
    sendMessage({
      'action': 'attack',
      'session_id': _sessionId,
      'user_id': _userId,
      'attacker_hand_index': attackerIndex,
      'target_hand_index': targetIndex,
    });
  }

  // Send end turn action
  void endTurn() {
    sendMessage({
      'action': 'end_turn',
      'session_id': _sessionId,
      'user_id': _userId,
    });
  }

  // Send surrender action
  void surrender() {
    sendMessage({
      'action': 'surrender',
      'session_id': _sessionId,
      'user_id': _userId,
    });
  }

  // Request battle state
  void requestBattleState() {
    sendMessage({
      'action': 'get_battle_state',
      'session_id': _sessionId,
      'user_id': _userId,
    });
  }

  // Send join session signal when joining battle
  void sendReady() {
    print('[WebSocketService] Sending join_session message');
    final message = {
      'action': 'join_session',
      'session_id': _sessionId,
      'user_id': _userId,
    };
    print('[WebSocketService] Message to send: $message');
    sendMessage(message);
  }

  // Disconnect
  void disconnect() {
    print('WebSocketService: Disconnecting...');
    _isConnected = false;

    try {
      if (_channel != null) {
        _channel!.sink.close(status.goingAway);
        _channel = null;
      }
    } catch (e) {
      print('WebSocketService: Error closing channel: $e');
    }

    try {
      if (_messageController != null) {
        _messageController!.close();
        _messageController = null;
      }
    } catch (e) {
      print('WebSocketService: Error closing message controller: $e');
    }

    _sessionId = null;
    _userId = null;
    print('WebSocketService: Disconnected successfully');
  }

  // Dispose
  void dispose() {
    print('WebSocketService: Disposing...');
    disconnect();
  }
}
