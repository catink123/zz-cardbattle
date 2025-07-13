import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../providers/game_provider.dart';
import '../main.dart';
import 'package:shared_preferences/shared_preferences.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final GlobalKey<ScaffoldState> _scaffoldKey = GlobalKey<ScaffoldState>();
  final GlobalKey<NavigatorState> _navigatorKey = GlobalKey<NavigatorState>();
  String _serverIp = 'localhost';

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _loadUserData();
      _loadServerIp();
    });
  }

  @override
  void dispose() {
    // Stop polling when leaving the home screen
    final gameProvider = context.read<GameProvider>();
    gameProvider.stopSessionPolling();
    super.dispose();
  }

  void _loadUserData() {
    final authProvider = context.read<AuthProvider>();
    final gameProvider = context.read<GameProvider>();

    if (authProvider.token != null && authProvider.userId != null) {
      gameProvider.setAuthInfo(authProvider.token!, authProvider.userId!);
      gameProvider.getSessions(); // Load available sessions
      gameProvider.startSessionPolling(); // Start polling for session updates
    }
  }

  Future<void> _loadServerIp() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _serverIp = prefs.getString('server_ip') ?? 'localhost';
    });
  }

  Future<void> _showServerIpDialog() async {
    final controller = TextEditingController(text: _serverIp);
    final result = await showDialog<String>(
      context: context,
      builder: (context) {
        return AlertDialog(
          backgroundColor: Theme.of(context).cardColor,
          title: const Text('Server IP Address'),
          content: TextField(
            controller: controller,
            decoration: const InputDecoration(
              labelText: 'Server IP',
              hintText: 'localhost',
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Cancel'),
            ),
            ElevatedButton(
              onPressed: () =>
                  Navigator.of(context).pop(controller.text.trim()),
              child: const Text('Save'),
            ),
          ],
        );
      },
    );
    if (result != null && result.isNotEmpty) {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('server_ip', result);
      setState(() {
        _serverIp = result;
      });
      // Optionally, show a snackbar
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Server IP set to $_serverIp')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      key: _scaffoldKey,
      appBar: AppBar(
        title: const Text('Card Battle - Home'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Server Settings',
            onPressed: _showServerIpDialog,
          ),
          Consumer<AuthProvider>(
            builder: (context, auth, child) {
              return PopupMenuButton<String>(
                onSelected: (value) {
                  if (value == 'logout') {
                    auth.logout();
                    Navigator.of(context).pushReplacementNamed('/login');
                  }
                },
                itemBuilder: (context) => [
                  PopupMenuItem(
                    value: 'logout',
                    child: const Row(
                      children: [
                        Icon(Icons.logout),
                        SizedBox(width: 8),
                        Text('Logout'),
                      ],
                    ),
                  ),
                ],
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const Icon(Icons.person),
                      const SizedBox(width: 8),
                      Text(auth.username ?? 'User'),
                    ],
                  ),
                ),
              );
            },
          ),
        ],
      ),
      body: Consumer<GameProvider>(
        builder: (context, game, child) {
          if (game.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          return Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Welcome section
                Card(
                  color: Theme.of(context).cardColor,
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Welcome to Card Battle!',
                          style: Theme.of(context).textTheme.headlineSmall
                              ?.copyWith(color: Colors.white),
                        ),
                        const SizedBox(height: 8),
                        const Text(
                          'Create sessions, join battles, and become the ultimate card master!',
                          style: TextStyle(color: Colors.white70),
                        ),
                        const SizedBox(height: 8),
                        Text(
                          'Server: $_serverIp',
                          style: const TextStyle(
                            color: Colors.blueAccent,
                            fontSize: 14,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),

                // Quick actions
                Text(
                  'Quick Actions',
                  style: Theme.of(
                    context,
                  ).textTheme.titleLarge?.copyWith(color: Colors.white),
                ),
                const SizedBox(height: 16),
                Row(
                  children: [
                    Expanded(
                      child: ElevatedButton.icon(
                        onPressed: () => _createSession(context),
                        icon: const Icon(Icons.add),
                        label: const Text('Create Session'),
                        style: ElevatedButton.styleFrom(
                          padding: const EdgeInsets.symmetric(vertical: 16),
                          backgroundColor: Colors.blue,
                          foregroundColor: Colors.white,
                        ),
                      ),
                    ),
                    const SizedBox(width: 16),
                    Expanded(
                      child: ElevatedButton.icon(
                        onPressed: () => _showJoinSessionDialog(context),
                        icon: const Icon(Icons.join_full),
                        label: const Text('Join Session'),
                        style: ElevatedButton.styleFrom(
                          padding: const EdgeInsets.symmetric(vertical: 16),
                          backgroundColor: Colors.blue,
                          foregroundColor: Colors.white,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 32),

                // Available sessions section
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Row(
                      children: [
                        Text(
                          'Available Sessions',
                          style: Theme.of(
                            context,
                          ).textTheme.titleLarge?.copyWith(color: Colors.white),
                        ),
                        if (game.isPolling) ...[
                          const SizedBox(width: 8),
                          const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          ),
                        ],
                      ],
                    ),
                    ElevatedButton.icon(
                      onPressed: () => _refreshSessions(context),
                      icon: const Icon(Icons.refresh),
                      label: const Text('Refresh'),
                    ),
                  ],
                ),
                const SizedBox(height: 16),

                // Error message
                if (game.error != null)
                  Container(
                    width: double.infinity,
                    padding: const EdgeInsets.all(12),
                    margin: const EdgeInsets.only(bottom: 16),
                    decoration: BoxDecoration(
                      color: Colors.red[900]?.withOpacity(0.2),
                      border: Border.all(color: Colors.redAccent),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Row(
                      children: [
                        const Icon(Icons.error, color: Colors.redAccent),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            game.error!,
                            style: const TextStyle(color: Colors.redAccent),
                          ),
                        ),
                        IconButton(
                          onPressed: () => game.clearError(),
                          icon: const Icon(
                            Icons.close,
                            color: Colors.redAccent,
                          ),
                        ),
                      ],
                    ),
                  ),

                // Sessions list
                Expanded(
                  child: game.sessions.isEmpty
                      ? const Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Icon(Icons.games, size: 64, color: Colors.grey),
                              SizedBox(height: 16),
                              Text(
                                'No sessions available',
                                style: TextStyle(
                                  fontSize: 18,
                                  color: Colors.grey,
                                ),
                              ),
                              SizedBox(height: 8),
                              Text(
                                'Create a session to start playing!',
                                style: TextStyle(color: Colors.grey),
                              ),
                            ],
                          ),
                        )
                      : ListView.builder(
                          itemCount: game.sessions.length,
                          itemBuilder: (context, index) {
                            final session = game.sessions[index];
                            return Card(
                              color: Theme.of(context).cardColor,
                              margin: const EdgeInsets.only(bottom: 8),
                              child: ListTile(
                                leading: Icon(
                                  session['status'] == 'waiting'
                                      ? Icons.hourglass_empty
                                      : Icons.games,
                                  color: session['status'] == 'waiting'
                                      ? Colors.orange
                                      : Colors.green,
                                ),
                                title: Text('Session ${session['id']}'),
                                subtitle: Text(
                                  'Status: ${session['status']} • Host: ${session['host_id']}',
                                ),
                                trailing: session['status'] == 'waiting'
                                    ? ElevatedButton(
                                        onPressed: () => _joinSession(
                                          context,
                                          session['id'],
                                        ),
                                        child: const Text('Join'),
                                      )
                                    : const Text('Ready'),
                              ),
                            );
                          },
                        ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  void _createSession(BuildContext context) async {
    // Get the game provider before any async operations
    final gameProvider = Provider.of<GameProvider>(context, listen: false);

    // Stop session polling immediately to prevent UI updates during navigation
    gameProvider.stopSessionPolling();

    final success = await gameProvider.createSession();

    if (success && mounted) {
      // Navigate to battle screen using global navigator key
      CardBattleApp.navigatorKey.currentState?.pushNamed(
        '/battle',
        arguments: gameProvider.currentSessionId,
      );
    } else if (mounted) {
      if (_scaffoldKey.currentContext != null) {
        ScaffoldMessenger.of(_scaffoldKey.currentContext!).showSnackBar(
          SnackBar(
            content: Text('Failed to create session: ${gameProvider.error}'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  void _showJoinSessionDialog(BuildContext context) {
    final sessionIdController = TextEditingController();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Join Session'),
        content: TextField(
          controller: sessionIdController,
          decoration: const InputDecoration(
            labelText: 'Session ID',
            hintText: 'Enter session ID',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              final sessionId = sessionIdController.text.trim();
              if (sessionId.isNotEmpty) {
                Navigator.of(context).pop();
                _joinSession(context, sessionId);
              }
            },
            child: const Text('Join'),
          ),
        ],
      ),
    );
  }

  void _joinSession(BuildContext context, String sessionId) async {
    // Get the game provider before any async operations
    final gameProvider = Provider.of<GameProvider>(context, listen: false);

    // Stop session polling immediately to prevent UI updates during navigation
    gameProvider.stopSessionPolling();

    final success = await gameProvider.joinSession(sessionId);

    if (success && mounted) {
      // Navigate to battle screen using global navigator key
      CardBattleApp.navigatorKey.currentState?.pushNamed(
        '/battle',
        arguments: sessionId,
      );
    } else if (mounted) {
      if (_scaffoldKey.currentContext != null) {
        ScaffoldMessenger.of(_scaffoldKey.currentContext!).showSnackBar(
          SnackBar(
            content: Text('Failed to join session: ${gameProvider.error}'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  void _refreshSessions(BuildContext context) async {
    final gameProvider = context.read<GameProvider>();
    await gameProvider.getSessions();

    if (mounted && _scaffoldKey.currentContext != null) {
      ScaffoldMessenger.of(_scaffoldKey.currentContext!).showSnackBar(
        const SnackBar(
          content: Text('Sessions refreshed'),
          backgroundColor: Colors.blue,
        ),
      );
    }
  }
}
