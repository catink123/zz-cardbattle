import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../providers/game_provider.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _loadUserData();
    });
  }

  void _loadUserData() {
    final authProvider = context.read<AuthProvider>();
    final gameProvider = context.read<GameProvider>();

    if (authProvider.token != null && authProvider.userId != null) {
      gameProvider.setAuthInfo(authProvider.token!, authProvider.userId!);
      gameProvider.loadDecks();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Card Battle - Home'),
        backgroundColor: Colors.blue,
        foregroundColor: Colors.white,
        actions: [
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

          return Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Welcome section
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Welcome to Card Battle!',
                          style: Theme.of(context).textTheme.headlineSmall,
                        ),
                        const SizedBox(height: 8),
                        const Text(
                          'Create decks, join battles, and become the ultimate card master!',
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),

                // Quick actions
                Text(
                  'Quick Actions',
                  style: Theme.of(context).textTheme.titleLarge,
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
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 32),

                // Decks section
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      'Your Decks',
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    ElevatedButton.icon(
                      onPressed: () => _showCreateDeckDialog(context),
                      icon: const Icon(Icons.add),
                      label: const Text('Create Deck'),
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
                      color: Colors.red[50],
                      border: Border.all(color: Colors.red),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Row(
                      children: [
                        const Icon(Icons.error, color: Colors.red),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            game.error!,
                            style: const TextStyle(color: Colors.red),
                          ),
                        ),
                        IconButton(
                          onPressed: () => game.clearError(),
                          icon: const Icon(Icons.close, color: Colors.red),
                        ),
                      ],
                    ),
                  ),

                // Decks list
                Expanded(
                  child: game.decks.isEmpty
                      ? const Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Icon(Icons.deck, size: 64, color: Colors.grey),
                              SizedBox(height: 16),
                              Text(
                                'No decks yet',
                                style: TextStyle(
                                  fontSize: 18,
                                  color: Colors.grey,
                                ),
                              ),
                              SizedBox(height: 8),
                              Text(
                                'Create your first deck to start playing!',
                                style: TextStyle(color: Colors.grey),
                              ),
                            ],
                          ),
                        )
                      : ListView.builder(
                          itemCount: game.decks.length,
                          itemBuilder: (context, index) {
                            final deck = game.decks[index];
                            return Card(
                              margin: const EdgeInsets.only(bottom: 8),
                              child: ListTile(
                                leading: const Icon(Icons.deck),
                                title: Text(deck['name'] ?? 'Unnamed Deck'),
                                subtitle: Text(
                                  '${deck['cards']?.length ?? 0} cards',
                                ),
                                trailing: PopupMenuButton<String>(
                                  onSelected: (value) {
                                    if (value == 'edit') {
                                      _showEditDeckDialog(context, deck);
                                    } else if (value == 'delete') {
                                      _showDeleteDeckDialog(context, deck);
                                    }
                                  },
                                  itemBuilder: (context) => [
                                    const PopupMenuItem(
                                      value: 'edit',
                                      child: Row(
                                        children: [
                                          Icon(Icons.edit),
                                          SizedBox(width: 8),
                                          Text('Edit'),
                                        ],
                                      ),
                                    ),
                                    const PopupMenuItem(
                                      value: 'delete',
                                      child: Row(
                                        children: [
                                          Icon(Icons.delete, color: Colors.red),
                                          SizedBox(width: 8),
                                          Text(
                                            'Delete',
                                            style: TextStyle(color: Colors.red),
                                          ),
                                        ],
                                      ),
                                    ),
                                  ],
                                ),
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
    final authProvider = context.read<AuthProvider>();
    final gameProvider = context.read<GameProvider>();

    if (authProvider.token != null && authProvider.userId != null) {
      gameProvider.setAuthInfo(authProvider.token!, authProvider.userId!);
      final success = await gameProvider.createSession();
      if (success && mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              'Session created! ID: ${gameProvider.currentSessionId}',
            ),
            action: SnackBarAction(
              label: 'Copy',
              onPressed: () {
                // TODO: Copy session ID to clipboard
              },
            ),
          ),
        );
        Navigator.of(
          context,
        ).pushNamed('/battle', arguments: gameProvider.currentSessionId);
      }
    }
  }

  void _showJoinSessionDialog(BuildContext context) {
    final sessionController = TextEditingController();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Join Session'),
        content: TextField(
          controller: sessionController,
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
            onPressed: () async {
              final sessionId = sessionController.text.trim();
              if (sessionId.isNotEmpty) {
                Navigator.of(context).pop();

                final authProvider = context.read<AuthProvider>();
                final gameProvider = context.read<GameProvider>();

                if (authProvider.token != null && authProvider.userId != null) {
                  gameProvider.setAuthInfo(
                    authProvider.token!,
                    authProvider.userId!,
                  );
                  final success = await gameProvider.joinSession(sessionId);
                  if (success && mounted) {
                    Navigator.of(
                      context,
                    ).pushNamed('/battle', arguments: sessionId);
                  } else if (mounted) {
                    // Show error feedback
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                        content: Text(
                          gameProvider.error ?? 'Failed to join session',
                        ),
                        backgroundColor: Colors.red,
                      ),
                    );
                  }
                }
              }
            },
            child: const Text('Join'),
          ),
        ],
      ),
    );
  }

  void _showCreateDeckDialog(BuildContext context) {
    final nameController = TextEditingController();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Create Deck'),
        content: TextField(
          controller: nameController,
          decoration: const InputDecoration(
            labelText: 'Deck Name',
            hintText: 'Enter deck name',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () async {
              final name = nameController.text.trim();
              if (name.isNotEmpty) {
                Navigator.of(context).pop();

                final authProvider = context.read<AuthProvider>();
                final gameProvider = context.read<GameProvider>();

                if (authProvider.token != null && authProvider.userId != null) {
                  gameProvider.setAuthInfo(
                    authProvider.token!,
                    authProvider.userId!,
                  );
                  // TODO: Add card selection
                  await gameProvider.createDeck(name, []);
                }
              }
            },
            child: const Text('Create'),
          ),
        ],
      ),
    );
  }

  void _showEditDeckDialog(BuildContext context, Map<String, dynamic> deck) {
    final nameController = TextEditingController(text: deck['name'] ?? '');

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Edit Deck'),
        content: TextField(
          controller: nameController,
          decoration: const InputDecoration(
            labelText: 'Deck Name',
            hintText: 'Enter new deck name',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () async {
              final newName = nameController.text.trim();
              if (newName.isNotEmpty) {
                Navigator.of(context).pop();

                final authProvider = context.read<AuthProvider>();
                final gameProvider = context.read<GameProvider>();

                if (authProvider.token != null && authProvider.userId != null) {
                  gameProvider.setAuthInfo(
                    authProvider.token!,
                    authProvider.userId!,
                  );
                  await gameProvider.updateDeck(
                    deck['id'],
                    newName,
                    deck['card_ids'] ?? [],
                  );
                }
              }
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );
  }

  void _showDeleteDeckDialog(BuildContext context, Map<String, dynamic> deck) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Deck'),
        content: Text(
          'Are you sure you want to delete the deck "${deck['name'] ?? 'Unnamed Deck'}"?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () async {
              Navigator.of(context).pop();

              final authProvider = context.read<AuthProvider>();
              final gameProvider = context.read<GameProvider>();

              if (authProvider.token != null && authProvider.userId != null) {
                gameProvider.setAuthInfo(
                  authProvider.token!,
                  authProvider.userId!,
                );
                // TODO: Implement deck deletion
                await gameProvider.deleteDeck(deck['id']);
              }
            },
            child: const Text('Delete'),
          ),
        ],
      ),
    );
  }
}
