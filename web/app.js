let currentChatId = null;
let currentChatType = null; // 'friend', 'group', or 'conference'
let lastEventId = 0;
let pollTimeout = null;

// Contact list data
let contacts = {
    friends: [],
    groups: [],
    conferences: []
};

// Load self info
function loadSelfInfo() {
    fetch('/api/self')
        .then(r => r.json())
        .then(data => {
            selfInfo = data;
            const connClass = data.connection_status === 'offline' ? 'offline' : 'online';
            document.getElementById('selfInfo').innerHTML = `
                <div class="name">${data.name || '未设置名称'}</div>
                <div class="status-message">${data.status_message || '无状态消息'}</div>
                <div class="address">${data.address}</div>
                <div class="status ${connClass}">连接: ${data.connection_status}</div>
            `;
        })
        .catch(err => {
            console.error('loadSelfInfo error:', err);
            document.getElementById('selfInfo').innerHTML = '加载失败';
        });
}

// Load all contacts (friends, groups, conferences) and merge into single list
function loadContacts(filter = 'all') {
    console.log('Loading contacts, filter:', filter);
    
    // Load friends
    fetch('/api/friends')
        .then(r => r.json())
        .then(data => {
            contacts.friends = data.friends || [];
            // Load details for each friend
            const promises = contacts.friends.map(f => 
                fetch('/api/friend', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `friend_id=${f}`
                }).then(r => r.json())
            );
            return Promise.all(promises);
        })
        .then(friendDetails => {
            contacts.friends = friendDetails;
            // Load groups
            return fetch('/api/groups').then(r => r.json());
        })
        .then(data => {
            contacts.groups = data.groups || [];
            // Load conferences
            return fetch('/api/conferences').then(r => r.json());
        })
        .then(data => {
            contacts.conferences = data.conferences || [];
            // Render merged list
            renderContactList(filter);
        })
        .catch(err => {
            console.error('loadContacts error:', err);
            document.getElementById('contactList').innerHTML = 
                '<div style="padding:10px;color:#f85149;">加载联系人失败</div>';
        });
}

// Render merged contact list with emoji indicators
function renderContactList(filter) {
    const list = document.getElementById('contactList');
    let html = '';
    
    // Add friends
    if (filter === 'all' || filter === 'friends') {
        contacts.friends.forEach(f => {
            const isSelected = f.friend_id == currentChatId && currentChatType === 'friend';
            const dotClass = f.connection_status === 'offline' ? 'offline-dot' : 'online-dot';
            const emoji = '👤';
            // Show name, or public key first 7 chars if name empty
            let displayName = f.name;
            if (!displayName || displayName === '') {
                displayName = (f.public_key || '').substring(0, 7) + '...';
            }
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-friend-id="${f.friend_id}" onclick="selectContact(${f.friend_id}, 'friend')">
                    <span class="${dotClass}"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">${displayName}</span>
                </div>
            `;
        });
    }
    
    // Add groups
    if (filter === 'all' || filter === 'groups') {
        contacts.groups.forEach(g => {
            const isSelected = g == currentChatId && currentChatType === 'group';
            const emoji = '👥';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" onclick="selectContact(${g}, 'group')">
                    <span class="group-dot"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">群组 ${g}</span>
                </div>
            `;
        });
    }
    
    // Add conferences
    if (filter === 'all' || filter === 'conferences') {
        contacts.conferences.forEach(c => {
            const isSelected = c == currentChatId && currentChatType === 'conference';
            const emoji = '🎙';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" onclick="selectContact(${c}, 'conference')">
                    <span class="conference-dot"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">会议 ${c}</span>
                </div>
            `;
        });
    }
    
    if (html === '') {
        list.innerHTML = '<div style="padding:10px;color:#6e7681;">暂无联系人</div>';
    } else {
        list.innerHTML = html;
    }
}

// Tab switching
function showTab(tab) {
    currentChatType = tab;
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    event.target.classList.add('active');

    document.getElementById('add-friend-area').classList.toggle('hidden', tab !== 'friends' && tab !== 'all');
    document.getElementById('add-group-area').classList.toggle('hidden', tab !== 'groups');
    document.getElementById('add-conference-area').classList.toggle('hidden', tab !== 'conferences');

    loadContacts(tab);
}

// Select a contact (friend/group/conference)
function selectContact(id, type) {
    currentChatId = id;
    currentChatType = type;
    
    let headerText = '';
    if (type === 'friend') {
        headerText = `与好友 ${id} 聊天`;
    } else if (type === 'group') {
        headerText = `群组 ${id}`;
    } else if (type === 'conference') {
        headerText = `会议 ${id}`;
    }
    
    document.getElementById('chatHeader').textContent = headerText;
    document.getElementById('messageArea').innerHTML = '';
    
    // Refresh list to show selection
    renderContactList(currentChatType === 'friend' ? 'all' : 
                     currentChatType === 'group' ? 'groups' : 
                     currentChatType === 'conference' ? 'conferences' : 'all');
}

// Long polling for events
function longPollEvents() {
    fetch(`/api/events?after=${lastEventId}`)
        .then(r => r.json())
        .then(events => {
            if (events && events.length > 0) {
                events.forEach(event => {
                    console.log('Event:', event);
                    lastEventId = event.event_id;
                    
                    if (event.event_type === 'friend_message') {
                        const data = JSON.parse(event.data);
                        if (data.friend_id == currentChatId && currentChatType === 'friend') {
                            appendMessage(data.message, 'other', data.friend_id);
                        }
                    } else if (event.event_type === 'friend_name' || event.event_type === 'friend_status') {
                        // Friend info updated, refresh contacts
                        loadContacts(currentChatType === 'friend' ? 'all' : 
                                     currentChatType === 'group' ? 'groups' : 
                                     currentChatType === 'conference' ? 'conferences' : 'all');
                    } else if (event.event_type === 'connection_status') {
                        loadSelfInfo();
                    }
                });
                // Continue polling immediately if we got events
                longPollEvents();
            } else {
                // No events, wait 2 seconds before retrying
                pollTimeout = setTimeout(longPollEvents, 2000);
            }
        })
        .catch(err => {
            console.error('Long poll error:', err);
            pollTimeout = setTimeout(longPollEvents, 3000);
        });
}

// Append message to chat area
function appendMessage(text, type, senderId) {
    const msgDiv = document.createElement('div');
    msgDiv.className = 'message ' + type;
    if (type === 'other') {
        msgDiv.innerHTML = `<div class="sender">好友 ${senderId}</div>${escapeHtml(text)}`;
    } else {
        msgDiv.textContent = text;
    }
    const area = document.getElementById('messageArea');
    area.appendChild(msgDiv);
    area.scrollTop = area.scrollHeight;
}

// Escape HTML
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Send message
function sendMessage() {
    if (!currentChatId || !currentChatType) {
        alert('请先选择聊天对象');
        return;
    }
    const input = document.getElementById('messageInput');
    const msg = input.value.trim();
    if (!msg) return;

    let url, body;
    if (currentChatType === 'friend') {
        url = '/api/messages';
        body = `friend_id=${currentChatId}&message=${encodeURIComponent(msg)}`;
    } else if (currentChatType === 'group') {
        url = '/api/group_messages';
        body = `group_id=${currentChatId}&message=${encodeURIComponent(msg)}`;
    } else if (currentChatType === 'conference') {
        url = '/api/conference_messages';
        body = `conference_id=${currentChatId}&message=${encodeURIComponent(msg)}`;
    }
    
    fetch(url, {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: body
    }).then(() => {
        appendMessage(msg, 'self');
        input.value = '';
    }).catch(err => {
        console.error('Send error:', err);
        alert('发送失败');
    });
}

// Add friend
function addFriend() {
    const input = document.getElementById('addFriendInput');
    const pubkey = input.value.trim();
    if (!pubkey || (pubkey.length !== 64 && pubkey.length !== 76)) {
        alert('请输入有效的公钥 (64字符) 或地址 (76字符)');
        return;
    }
    fetch('/api/friends', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `public_key=${encodeURIComponent(pubkey)}`
    }).then(r => r.json())
      .then(data => {
          alert('添加成功！');
          input.value = '';
          loadContacts('friends');
      }).catch(err => {
          alert('添加失败: ' + err);
      });
}

// Create group
function createGroup() {
    fetch('/api/groups', {
        method: 'POST'
    }).then(r => r.json())
      .then(data => {
          alert('群组创建成功！');
          loadContacts('groups');
      }).catch(err => {
          alert('创建失败: ' + err);
      });
}

// Create conference
function createConference() {
    fetch('/api/conferences', {
        method: 'POST'
    }).then(r => r.json())
      .then(data => {
          alert('会议创建成功！');
          loadContacts('conferences');
      }).catch(err => {
          alert('创建失败: ' + err);
      });
}

// Friend context menu
let selectedFriendId = null;

// Use event delegation for friend list context menu
document.addEventListener('DOMContentLoaded', () => {
    const contactList = document.getElementById('contactList');
    if (contactList) {
        contactList.addEventListener('contextmenu', (event) => {
            const item = event.target.closest('.list-item');
            if (item && item.dataset.friendId) {
                event.preventDefault();
                selectedFriendId = item.dataset.friendId;
                const menu = document.getElementById('friendMenu');
                menu.style.display = 'block';
                menu.style.left = event.pageX + 'px';
                menu.style.top = event.pageY + 'px';
                
                // Hide menu when clicking elsewhere
                setTimeout(() => {
                    document.addEventListener('click', hideFriendMenu, { once: true });
                }, 0);
            }
        });
    }

    // Handle menu item clicks
    const menu = document.getElementById('friendMenu');
    if (menu) {
        menu.addEventListener('click', (event) => {
            const action = event.target.getAttribute('data-action');
            if (action === 'info') {
                window.showFriendInfo(selectedFriendId);
            } else if (action === 'delete') {
                window.deleteFriend();
            }
        });
    }

    hideFriendMenu();
});

function hideFriendMenu() {
    const menu = document.getElementById('friendMenu');
    if (menu) menu.style.display = 'none';
}

function showFriendInfo(friendId) {
    fetch('/api/friend', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${friendId}`
    }).then(r => r.json())
      .then(data => {
          document.getElementById('infoFriendName').textContent = data.name || '无名';
          document.getElementById('infoFriendId').textContent = data.friend_id;
          document.getElementById('infoFriendStatus').textContent = data.status || '未知';
          document.getElementById('infoFriendConn').textContent = data.connection_status || '未知';
          document.getElementById('infoFriendPk').textContent = data.public_key || '未知';
          
          document.getElementById('friendInfoModal').classList.remove('hidden');
      });
    hideFriendMenu();
}

function hideFriendInfo() {
    document.getElementById('friendInfoModal').classList.add('hidden');
}

function deleteFriend() {
    if (!selectedFriendId) return;
    
    if (!confirm('确定要删除好友 ' + selectedFriendId + ' 吗？')) {
        return;
    }
    
    fetch('/api/friend_delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${selectedFriendId}`
    }).then(r => r.json())
      .then(data => {
          alert('好友已删除');
          loadContacts('friends');
      }).catch(err => {
          alert('删除失败: ' + err);
      });
    
    hideFriendMenu();
}

// Show edit self modal
function showEditSelf() {
    fetch('/api/self')
        .then(r => r.json())
        .then(data => {
            document.getElementById('editName').value = data.name || '';
            document.getElementById('editStatus').value = data.status_message || '';
            document.getElementById('editSelfModal').classList.remove('hidden');
        });
}

function hideEditSelf() {
    document.getElementById('editSelfModal').classList.add('hidden');
}

function saveSelfInfo() {
    const name = document.getElementById('editName').value.trim();
    const status = document.getElementById('editStatus').value.trim();
    
    const promises = [];
    if (name) {
        promises.push(
            fetch('/api/self/name', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `name=${encodeURIComponent(name)}`
            })
        );
    }
    if (status) {
        promises.push(
            fetch('/api/self/status', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `status_message=${encodeURIComponent(status)}`
            })
        );
    }
    
    Promise.all(promises)
        .then(() => {
            hideEditSelf();
            loadSelfInfo();
        })
        .catch(err => {
            alert('保存失败: ' + err);
        });
}

// Bootstrap
function bootstrap() {
    fetch('/api/bootstrap', {
        method: 'POST'
    }).then(() => {
        alert('正在连接网络...');
        setTimeout(loadSelfInfo, 2000);
    }).catch(err => {
        alert('连接失败: ' + err);
    });
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM loaded, initializing...');
    loadSelfInfo();
    loadContacts('all');
    longPollEvents();
});

// Periodically refresh self info
setInterval(() => {
    loadSelfInfo();
}, 5000);

// Enter key sends message
document.getElementById('messageInput').addEventListener('keypress', e => {
    if (e.key === 'Enter') sendMessage();
});

// ESC key closes modals
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        hideFriendInfo();
        hideFriendMenu();
        hideEditSelf();
    }
});

// Expose functions to window for HTML onclick calls
window.showTab = showTab;
window.selectContact = selectContact;
window.sendMessage = sendMessage;
window.addFriend = addFriend;
window.createGroup = createGroup;
window.createConference = createConference;
window.showEditSelf = showEditSelf;
window.saveSelfInfo = saveSelfInfo;
window.hideEditSelf = hideEditSelf;
window.bootstrap = bootstrap;
window.showFriendInfo = showFriendInfo;
window.deleteFriend = deleteFriend;
window.hideFriendInfo = hideFriendInfo;
window.hideFriendMenu = hideFriendMenu;
