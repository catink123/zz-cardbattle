import pytest
import asyncio
import json

@pytest.mark.asyncio
async def test_battle_session_setup(service_client):
    # Test that sessions are properly set up for WebSocket battle flow
    import time
    timestamp = int(time.time())
    
    # Register two players
    resp1 = await service_client.post('/auth/register', json={
        'username': f'battle1_test_{timestamp}', 'email': f'battle1_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp1.status == 200
    resp1_data = resp1.json()
    assert resp1_data.get('success') == True
    token1 = resp1_data.get('token')
    assert token1

    resp2 = await service_client.post('/auth/register', json={
        'username': f'battle2_test_{timestamp}', 'email': f'battle2_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp2.status == 200
    resp2_data = resp2.json()
    assert resp2_data.get('success') == True
    token2 = resp2_data.get('token')
    assert token2

    # Player1 creates a session
    resp3 = await service_client.post('/game/create-session', 
                                    headers={'Authorization': f'Bearer {token1}'})
    assert resp3.status == 200
    session_data = resp3.json()
    assert session_data['success'] == True
    session_id = session_data['session_id']
    assert session_id

    # Player2 joins the session
    resp4 = await service_client.post('/game/join-session', 
                                    headers={'Authorization': f'Bearer {token2}'},
                                    json={'session_id': session_id})
    assert resp4.status == 200
    join_data = resp4.json()
    assert join_data['success'] == True
    assert join_data['session_status'] == 'ready'

    # Verify session is ready for WebSocket connection
    # The session should be in 'ready' state, which means both players have joined
    # and the battle can be started via WebSocket when players connect

@pytest.mark.asyncio
async def test_session_leave_flow(service_client):
    # Test that players can leave sessions properly
    import time
    timestamp = int(time.time())
    
    # Register players
    resp1 = await service_client.post('/auth/register', json={
        'username': f'leaver_test_{timestamp}', 'email': f'leaver_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp1.status == 200
    resp1_data = resp1.json()
    assert resp1_data.get('success') == True
    leaver_token = resp1_data.get('token')

    resp2 = await service_client.post('/auth/register', json={
        'username': f'stayer_test_{timestamp}', 'email': f'stayer_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp2.status == 200
    resp2_data = resp2.json()
    assert resp2_data.get('success') == True
    stayer_token = resp2_data.get('token')

    # Create session
    resp3 = await service_client.post('/game/create-session', 
                                    headers={'Authorization': f'Bearer {leaver_token}'})
    assert resp3.status == 200
    resp3_data = resp3.json()
    assert resp3_data.get('success') == True
    session_id = resp3_data['session_id']

    # Join session
    resp4 = await service_client.post('/game/join-session', 
                                    headers={'Authorization': f'Bearer {stayer_token}'},
                                    json={'session_id': session_id})
    assert resp4.status == 200

    # Leave session
    resp5 = await service_client.post('/game/leave-session', 
                                    headers={'Authorization': f'Bearer {leaver_token}'},
                                    json={'session_id': session_id})
    assert resp5.status == 200
    leave_data = resp5.json()
    assert leave_data['success'] == True

    # Verify session is no longer active (should be back in waiting or removed)
    resp6 = await service_client.get('/game/sessions')
    assert resp6.status == 200
    sessions_data = resp6.json()
    
    # The session should either be removed or back in waiting state
    session_found = False
    for session in sessions_data['sessions']:
        if session['id'] == session_id:
            session_found = True
            # If found, it should be back in waiting state
            assert session['status'] == 'waiting'
            break
    
    # Session might be removed entirely, which is also valid
    # We just verify the leave operation succeeded

@pytest.mark.asyncio
async def test_websocket_ready_session(service_client):
    # Test that a ready session is properly configured for WebSocket battle
    import time
    timestamp = int(time.time())
    
    # Register players
    resp1 = await service_client.post('/auth/register', json={
        'username': f'ws1_test_{timestamp}', 'email': f'ws1_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp1.status == 200
    resp1_data = resp1.json()
    assert resp1_data.get('success') == True
    token1 = resp1_data.get('token')

    resp2 = await service_client.post('/auth/register', json={
        'username': f'ws2_test_{timestamp}', 'email': f'ws2_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp2.status == 200
    resp2_data = resp2.json()
    assert resp2_data.get('success') == True
    token2 = resp2_data.get('token')

    # Create and join session
    resp3 = await service_client.post('/game/create-session', 
                                    headers={'Authorization': f'Bearer {token1}'})
    assert resp3.status == 200
    resp3_data = resp3.json()
    assert resp3_data.get('success') == True
    session_id = resp3_data['session_id']

    resp4 = await service_client.post('/game/join-session', 
                                    headers={'Authorization': f'Bearer {token2}'},
                                    json={'session_id': session_id})
    assert resp4.status == 200
    join_data = resp4.json()
    assert join_data['success'] == True
    assert join_data['session_status'] == 'ready'

    # At this point, the session is ready for WebSocket connection
    # Both players can connect via WebSocket and start the battle
    # The WebSocket handler will automatically start the battle when players join
    
    # Verify session is still in session list but with ready status
    resp5 = await service_client.get('/game/sessions')
    assert resp5.status == 200
    sessions_data = resp5.json()
    
    session_found = False
    for session in sessions_data['sessions']:
        if session['id'] == session_id:
            session_found = True
            assert session['status'] == 'ready'  # Session should be ready
            break
    assert session_found  # Session should still be in the list 