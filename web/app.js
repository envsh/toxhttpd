let currentFriendId = null;
let currentChatType = 'friend'; // 'friend' or 'group'
let sse = null;
let selfInfo = null;

// 加载自己信息
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
        });
}

loadSelfInfo();

// Tab切换
function showTab(tab) {
    currentChatType = tab;
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    event.target.classList.add('active');
    document.getElementById('friendList').classList.toggle('hidden', tab !== 'friends');
    document.getElementById('groupList').classList.toggle('hidden', tab !== 'groups');
    document.getElementById('add-friend-area').classList.toggle('hidden', tab !== 'friends');
    document.getElementById('add-group-area').classList.toggle('hidden', tab !== 'groups');
    if (tab === 'friends') loadFriends();
    else loadGroups();
}

// 加载好友列表
function loadFriends() {
    fetch('/api/friends')
        .then(r => r.json())
        .then(data => {
            const list = document.getElementById('friendList');
            if (!data.friends || data.friends.length === 0) {
                list.innerHTML = '<div style="padding:10px;color:#6e7681;">暂无好友</div>';
                return;
            }
            // 获取每个好友的详细信息
            const promises = data.friends.map(f => 
                fetch('/api/friend', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `friend_id=${f}`
                }).then(r => r.json())
            );
            Promise.all(promises).then(friends => {
                list.innerHTML = friends.map(f => {
                    const isSelected = f.friend_id == currentFriendId;
                    const dotClass = f.connection_status === 'offline' ? 'offline-dot' : 'online-dot';
                    return `
                        <div class="list-item ${isSelected ? 'selected' : ''}" onclick="selectFriend(${f.friend_id})">
                            <span class="${dotClass}"></span>
                            <span class="item-text">${f.name || '好友 ' + f.friend_id}</span>
                        </div>
                    `;
                }).join('');
            });
        });
}

// 加载群组列表
function loadGroups() {
    fetch('/api/groups')
        .then(r => r.json())
        .then(data => {
            const list = document.getElementById('groupList');
            if (!data.groups || data.groups.length === 0) {
                list.innerHTML = '<div style="padding:10px;color:#6e7681;">暂无群组</div>';
                return;
            }
            list.innerHTML = data.groups.map(g => `
                <div class="list-item" onclick="selectGroup(${g})">
                    <span class="group-dot"></span>
                    <span class="item-text">群组 ${g}</span>
                </div>
            `).join('');
        });
}

// 选择好友聊天
function selectFriend(friendId) {
    currentFriendId = friendId;
    currentChatType = 'friend';
    document.getElementById('chatHeader').textContent = `与好友 ${friendId} 聊天`;
    document.getElementById('messageArea').innerHTML = '';
    loadFriends(); // 刷新选中状态
}

// 选择群组聊天
function selectGroup(groupId) {
    currentFriendId = groupId;
    currentChatType = 'group';
    document.getElementById('chatHeader').innerHTML = `
        群组 ${groupId}
        <button class="invite-btn" onclick="showInviteDialog(${groupId})">邀请好友</button>
    `;
    document.getElementById('messageArea').innerHTML = '';
}

// 显示邀请好友对话框
function showInviteDialog(groupId) {
    const friendList = document.getElementById('friendList');
    const friends = friendList.querySelectorAll('.list-item');
    if (friends.length === 0) {
        alert('请先添加好友');
        return;
    }
    
    let friendOptions = '';
    friends.forEach(f => {
        const friendId = f.getAttribute('onclick').match(/\d+/)[0];
        const friendName = f.querySelector('.item-text').textContent;
        friendOptions += `<option value="${friendId}">${friendName}</option>`;
    });
    
    const dialog = document.createElement('div');
    dialog.className = 'modal';
    dialog.innerHTML = `
        <div class="modal-content">
            <h3>邀请好友到群组</h3>
            <div class="form-group">
                <label>选择好友:</label>
                <select id="inviteFriendSelect" style="width:100%;padding:8px;background:#0d1117;border:1px solid #30363d;color:#c9d1d9;border-radius:4px;">
                    ${friendOptions}
                </select>
            </div>
            <div class="modal-actions">
                <button onclick="inviteToGroup(${groupId}, this)">邀请</button>
                <button onclick="this.closest('.modal').remove()">取消</button>
            </div>
        </div>
    `;
    document.body.appendChild(dialog);
}

// 邀请好友到群组
function inviteToGroup(groupId, btn) {
    const select = document.getElementById('inviteFriendSelect');
    const friendId = select.value;
    
    fetch('/api/conference_invite', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${friendId}&conference_id=${groupId}`
    }).then(r => r.json())
      .then(data => {
          alert('邀请成功！');
          btn.closest('.modal').remove();
      }).catch(err => {
          alert('邀请失败: ' + err);
      });
}

// SSE接收实时消息和状态更新
function initSSE() {
    if (sse) sse.close();
    sse = new EventSource('/events/sse');
    sse.onmessage = (e) => {
        try {
            const event = JSON.parse(e.data);
            console.log('SSE event:', event);
            if (event.event_type === 'friend_message') {
                const data = JSON.parse(event.data);
                if (data.friend_id == currentFriendId && currentChatType === 'friend') {
                    appendMessage(data.message, 'other', data.friend_id);
                }
            } else if (event.event_type === 'friend_name' || event.event_type === 'friend_status') {
                // 好友信息更新，刷新列表
                if (currentChatType === 'friends') loadFriends();
            } else if (event.event_type === 'connection_status') {
                // 自己的连接状态更新
                loadSelfInfo();
            }
        } catch (err) {
            console.error('SSE parse error:', err);
        }
    };
    sse.onerror = () => {
        console.log('SSE connection lost, reconnecting...');
        setTimeout(initSSE, 3000); // 断线重连
    };
}
initSSE();

// 追加消息到聊天区域
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

// HTML转义
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// 发送消息
function sendMessage() {
    if (!currentFriendId) {
        alert('请先选择' + (currentChatType === 'friend' ? '好友' : '群组'));
        return;
    }
    const input = document.getElementById('messageInput');
    const msg = input.value.trim();
    if (!msg) return;

    const button = input.parentElement.querySelector('button');
    button.disabled = true;

    let url, body;
    if (currentChatType === 'friend') {
        url = '/api/messages';
        body = `friend_id=${currentFriendId}&message=${encodeURIComponent(msg)}`;
    } else {
        url = '/api/group_messages';
        body = `group_id=${currentFriendId}&message=${encodeURIComponent(msg)}`;
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
    }).finally(() => {
        button.disabled = false;
    });
}

// 添加好友
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
          loadFriends();
      }).catch(err => {
          alert('添加失败: ' + err);
      });
}

// 创建群组
function createGroup() {
    fetch('/api/groups', {
        method: 'POST'
    }).then(r => r.json())
      .then(data => {
          alert('群组创建成功！');
          loadGroups();
      }).catch(err => {
          alert('创建失败: ' + err);
      });
}

// 显示编辑自己信息模态框
function showEditSelf() {
    if (selfInfo) {
        document.getElementById('editName').value = selfInfo.name || '';
        document.getElementById('editStatus').value = selfInfo.status_message || '';
    }
    document.getElementById('editSelfModal').classList.remove('hidden');
}

// 隐藏编辑模态框
function hideEditSelf() {
    document.getElementById('editSelfModal').classList.add('hidden');
}

// 保存自己信息
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

// Bootstrap连接网络
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

// 回车发送
document.getElementById('messageInput').addEventListener('keypress', e => {
    if (e.key === 'Enter') sendMessage();
});

// 定时刷新
setInterval(() => {
    if (currentChatType === 'friends') loadFriends();
    loadSelfInfo();
}, 5000);
loadFriends();
