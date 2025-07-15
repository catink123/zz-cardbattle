import pytest
import asyncio
import json

@pytest.mark.asyncio
async def test_complete_game_flow(service_client):
    # Step 1: Register two players via HTTP
    import time
    timestamp = int(time.time())
    
    resp1 = await service_client.post('/auth/register', json={
        'username': f'player1_test_{timestamp}', 'email': f'player1_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp1.status == 200
    resp1_data = resp1.json()
    assert resp1_data.get('success') == True
    player1_token = resp1_data.get('token')
    assert player1_token

    resp2 = await service_client.post('/auth/register', json={
        'username': f'player2_test_{timestamp}', 'email': f'player2_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp2.status == 200
    resp2_data = resp2.json()
    assert resp2_data.get('success') == True
    player2_token = resp2_data.get('token')
    assert player2_token

    # Step 2: Create session via HTTP
    resp3 = await service_client.post('/game/create-session', 
                                    headers={'Authorization': f'Bearer {player1_token}'})
    assert resp3.status == 200
    session_data = resp3.json()
    assert session_data['success'] == True
    session_id = session_data['session_id']
    assert session_id

    # Step 3: Join session via HTTP
    resp4 = await service_client.post('/game/join-session', 
                                    headers={'Authorization': f'Bearer {player2_token}'},
                                    json={'session_id': session_id})
    assert resp4.status == 200
    join_data = resp4.json()
    assert join_data['success'] == True
    assert join_data['session_status'] == 'ready'

    # Step 4: Connect to WebSocket for real-time gameplay
    # Note: The actual WebSocket connection would be handled by the client
    # For testing purposes, we'll verify the session is ready for WebSocket connection
    resp5 = await service_client.get('/game/sessions')
    assert resp5.status == 200
    sessions_data = resp5.json()
    assert sessions_data['success'] == True
    
    # Verify our session is still in session list but with ready status
    waiting_sessions = sessions_data['sessions']
    session_found = False
    for session in waiting_sessions:
        if session['id'] == session_id:
            session_found = True
            assert session['status'] == 'ready'  # Session should be ready
            break
    # Session should still be in the list since both players joined
    assert session_found

@pytest.mark.asyncio
async def test_session_management_flow(service_client):
    # Test session creation and joining
    import time
    timestamp = int(time.time())
    
    resp1 = await service_client.post('/auth/register', json={
        'username': f'host_test_{timestamp}', 'email': f'host_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp1.status == 200
    resp1_data = resp1.json()
    assert resp1_data.get('success') == True
    host_token = resp1_data.get('token')

    # Create session
    resp2 = await service_client.post('/game/create-session', 
                                    headers={'Authorization': f'Bearer {host_token}'})
    assert resp2.status == 200
    session_data = resp2.json()
    assert session_data.get('success') == True
    session_id = session_data['session_id']

    # Check waiting sessions
    resp3 = await service_client.get('/game/sessions')
    assert resp3.status == 200
    sessions_data = resp3.json()
    assert sessions_data['success'] == True
    
    # Verify session is in waiting list
    session_found = False
    for session in sessions_data['sessions']:
        if session['id'] == session_id:
            session_found = True
            assert session['status'] == 'waiting'
            break
    assert session_found

    # Register guest and join
    resp4 = await service_client.post('/auth/register', json={
        'username': f'guest_test_{timestamp}', 'email': f'guest_test_{timestamp}@test.com', 'password': 'password123'
    })
    assert resp4.status == 200
    resp4_data = resp4.json()
    assert resp4_data.get('success') == True
    guest_token = resp4_data.get('token')

    resp5 = await service_client.post('/game/join-session', 
                                    headers={'Authorization': f'Bearer {guest_token}'},
                                    json={'session_id': session_id})
    assert resp5.status == 200
    join_data = resp5.json()
    assert join_data['success'] == True
    assert join_data['session_status'] == 'ready'

    # Verify session is still in session list but with ready status
    resp6 = await service_client.get('/game/sessions')
    assert resp6.status == 200
    sessions_data = resp6.json()
    session_found = False
    for session in sessions_data['sessions']:
        if session['id'] == session_id:
            session_found = True
            assert session['status'] == 'ready'  # Session should be ready
            break
    assert session_found  # Session should still be in the list 