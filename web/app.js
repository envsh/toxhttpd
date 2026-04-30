let currentChatId = null;
let currentChatType = null; // 'friend', 'group', or 'conference'
let lastEventId = 0;
let pollTimeout = null;
let selfAddress = ''; // 保存完整地址

// Contact list data
let contacts = {
    friends: [],
    groups: [],
    conferences: []
};

// 多语言支持
let currentLang = 'zh-CN'; // 默认简体中文
let langData = {}; // 语言数据

// 加载语言文件
async function loadLanguage(lang) {
    try {
        const response = await fetch(`/web/lang/${lang}.json`);
        langData = await response.json();
        currentLang = lang;
        // 保存到本地存储
        try {
            localStorage.setItem('toxchat_lang', lang);
        } catch(e) {
            console.warn('Failed to save language to localStorage:', e);
        }
        // 应用语言
        applyLanguage();
        console.log(`Language loaded: ${lang}`);
    } catch (err) {
        console.error('Failed to load language:', err);
    }
}

// 获取翻译文本，支持占位符 {0}, {1} 等
function t(key, ...args) {
    const keys = key.split('.');
    let result = langData;
    for (const k of keys) {
        if (result && result[k] !== undefined) {
            result = result[k];
        } else {
            console.warn(`Translation missing: ${key}`);
            return key; // 返回 key 作为后备
        }
    }
    // 替换占位符
    if (typeof result === 'string' && args.length > 0) {
        return result.replace(/\{(\d+)\}/g, (match, index) => {
            return args[index] !== undefined ? args[index] : match;
        });
    }
    return result;
}

// 应用语言到页面
function applyLanguage() {
    // 更新页面标题
    document.title = t('app_title');
    
    // 更新 Tab 按钮
    const tabs = document.querySelectorAll('.tab');
    if (tabs[0]) tabs[0].textContent = t('tabs.all');
    if (tabs[1]) tabs[1].textContent = t('tabs.friends');
    if (tabs[2]) tabs[2].textContent = t('tabs.groups');
    if (tabs[3]) tabs[3].textContent = t('tabs.conferences');
    
    // 更新操作按钮
    const actions = document.querySelectorAll('.self-actions .action-btn');
    if (actions[0]) actions[0].textContent = t('buttons.edit_info');
    if (actions[1]) actions[1].textContent = t('buttons.connect_network');
    if (actions[2]) actions[2].textContent = t('buttons.qrcode');
    
    // 更新底部添加好友区域
    const addInput = document.getElementById('addFriendInputBottom');
    if (addInput) addInput.placeholder = t('placeholders.add_friend');
    
    // 更新创建按钮（底部）
    const createBtns = document.querySelectorAll('.create-btn');
    if (createBtns[0]) createBtns[0].textContent = '🎥 ' + t('buttons.create_conference');
    if (createBtns[1]) createBtns[1].textContent = '👥 ' + t('buttons.create_group');
    
    // 更新发送按钮
    const sendBtn = document.querySelector('.input-area button');
    if (sendBtn) sendBtn.textContent = t('buttons.send');
    
    // 更新聊天头部
    const chatHeaderText = document.getElementById('chatHeaderText');
    if (chatHeaderText) {
        if (currentChatId === null || currentChatId === undefined) {
            chatHeaderText.textContent = t('select_chat_object');
        } else {
            // 重新生成当前聊天对象的头部文本（语言切换时更新）
            let headerText = '';
            if (currentChatType === 'friend') {
                headerText = t('chat_with_friend', currentChatId);
            } else if (currentChatType === 'group') {
                headerText = t('group') + ' ' + currentChatId;
            } else if (currentChatType === 'conference') {
                headerText = t('conference_item') + ' ' + currentChatId;
            }
            chatHeaderText.textContent = headerText;
        }
    }
    
    // 更新模态框
    updateModalTexts();
    
    // 更新右键菜单
    const menuItems = document.querySelectorAll('#friendMenu .menu-item');
    if (menuItems[0]) menuItems[0].textContent = t('context_menu.view_info');
    if (menuItems[1]) menuItems[1].textContent = t('context_menu.delete_friend');
    
    console.log('Language applied:', currentLang);
}

// 更新模态框文本
function updateModalTexts() {
    // 编辑信息模态框
    const editTitle = document.querySelector('#editSelfModal h3');
    if (editTitle) editTitle.textContent = t('modals.edit_info_title');
    
    const editLabels = document.querySelectorAll('#editSelfModal .form-group label');
    if (editLabels[0]) editLabels[0].textContent = t('modals.labels.name');
    if (editLabels[1]) editLabels[1].textContent = t('modals.labels.status_message');
    
    const editBtns = document.querySelectorAll('#editSelfModal .modal-actions button');
    if (editBtns[0]) editBtns[0].textContent = t('buttons.save');
    if (editBtns[1]) editBtns[1].textContent = t('buttons.cancel');
    
    // 好友信息模态框
    const friendTitle = document.querySelector('#friendInfoModal h3');
    if (friendTitle) friendTitle.textContent = t('modals.friend_info_title');
    
    const friendLabels = document.querySelectorAll('#friendInfoModal .info-row label');
    if (friendLabels[0]) friendLabels[0].textContent = t('modals.labels.name');
    if (friendLabels[1]) friendLabels[1].textContent = t('modals.labels.friend_id');
    if (friendLabels[2]) friendLabels[2].textContent = t('modals.labels.status');
    if (friendLabels[3]) friendLabels[3].textContent = t('modals.labels.connection');
    if (friendLabels[4]) friendLabels[4].textContent = t('modals.labels.public_key');
    
    const friendBtns = document.querySelectorAll('#friendInfoModal .modal-actions button');
    if (friendBtns[0]) friendBtns[0].textContent = t('buttons.close');
}

// 初始化语言
function initLanguage() {
    // 1. 先尝试从 localStorage 读取
    try {
        const savedLang = localStorage.getItem('toxchat_lang');
        if (savedLang && (savedLang === 'zh-CN' || savedLang === 'zh-TW' || savedLang === 'en-US')) {
            loadLanguage(savedLang);
            return;
        }
    } catch(e) {
        console.warn('Failed to read language from localStorage:', e);
    }
    
    // 2. 检测浏览器语言
    const browserLang = navigator.language || navigator.userLanguage;
    if (browserLang) {
        if (browserLang.toLowerCase().startsWith('zh')) {
            if (browserLang.toLowerCase().includes('tw') || browserLang.toLowerCase().includes('hk')) {
                loadLanguage('zh-TW'); // 繁体
            } else {
                loadLanguage('zh-CN'); // 简体（默认）
            }
        } else if (browserLang.toLowerCase().startsWith('en')) {
            loadLanguage('en-US');
        } else {
            loadLanguage('zh-CN'); // 默认简体
        }
    } else {
        loadLanguage('zh-CN'); // 默认简体
    }
}

// 切换语言函数（供选择器调用）
function switchLanguage(lang) {
    if (lang === 'zh-CN' || lang === 'zh-TW' || lang === 'en-US') {
        loadLanguage(lang);
    }
}

// Load self info
function loadSelfInfo() {
    fetch('/api/self')
        .then(r => r.json())
        .then(data => {
            selfInfo = data;
            selfAddress = data.address;

            const connStatus = data.connection_status === 'offline' ? 'offline' :
                               data.connection_status === 'tcp' ? 'tcp' : 'online';
            const connText = data.connection_status === 'offline' ? t('statuses.offline') :
                          data.connection_status === 'tcp' ? t('statuses.tcp') : t('statuses.udp');
            
            // 截断地址显示（前8后8）
            const shortAddr = data.address.length > 20 ? 
                data.address.substring(0, 8) + '...' + data.address.substring(data.address.length - 8) :
                data.address;
            
            // 更新头像（显示名称首字母）
            const avatar = document.getElementById('selfAvatar');
            const initial = (data.name || '?').charAt(0).toUpperCase();
            avatar.textContent = initial;
            if (data.name && data.name !== '') {
                avatar.style.background = '#1c3a5f';
                avatar.style.borderColor = '#00d4aa';
            } else {
                avatar.style.background = '#21262d';
                avatar.style.borderColor = '#30363d';
            }

            // 更新状态标识（名称右侧）
            const badge = document.getElementById('statusBadge');
            badge.className = 'self-status-badge ' + connStatus;
            badge.textContent = connText;
            
            // 更新名称
            document.getElementById('selfName').textContent = data.name || t('no_name');
            document.getElementById('selfStatusMessage').textContent = data.status_message || t('no_status');
            
            // 更新地址
            const addrElem = document.getElementById('selfAddress');
            addrElem.textContent = shortAddr;
            addrElem.title = data.address;
            addrElem.onclick = function() {
                copyToClipboard(data.address);
                alert(t('tox_id_copied'));
            };
        })
        .catch(err => {
            console.error('loadSelfInfo error:', err);
            const badge = document.getElementById('statusBadge');
            badge.className = 'self-status-badge offline';
            badge.textContent = t('load_failed');
            document.getElementById('selfAvatar').textContent = '?';
        });
}

// 复制完整地址
function copyAddress() {
    if (selfAddress) {
        copyToClipboard(selfAddress);
        alert(t('tox_id_copied'));
    }
}

// 复制到剪贴板通用函数
function copyToClipboard(text) {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    document.body.appendChild(textarea);
    textarea.select();
    document.execCommand('copy');
    document.body.removeChild(textarea);
}

// 显示二维码（可选）
function showQRCode() {
    if (!selfAddress) {
        alert(t('please_wait'));
        return;
    }
    // 简单实现：在新窗口打开二维码生成服务
    const qrUrl = `https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=${encodeURIComponent(selfAddress)}`;
    window.open(qrUrl, 'qrcode', 'width=250,height=300');
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
            console.log('Friend details loaded:', friendDetails);
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
                '<div style="padding:10px;color:#f85149;">' + t('load_contacts_failed') + '</div>';
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
                    <span class="item-text">${t('group')} ${g}</span>
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
                    <span class="item-text">${t('conference_item')} ${c}</span>
                </div>
            `;
        });
    }
    
    if (html === '') {
        list.innerHTML = '<div style="padding:10px;color:#6e7681;">' + t('no_contacts') + '</div>';
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
    console.log('selectContact called: id=' + id + ', type=' + type + ', id type=' + typeof id);
    currentChatId = id;
    currentChatType = type;
    console.log('After set: currentChatId=' + currentChatId + ', currentChatType=' + currentChatType);
    
    let headerText = '';
    if (type === 'friend') {
        headerText = t('chat_with_friend', id);
    } else if (type === 'group') {
        headerText = t('group') + ' ' + id;
    } else if (type === 'conference') {
        headerText = t('conference_item') + ' ' + id;
    }
    
    document.getElementById('chatHeaderText').textContent = headerText;
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
                    } else if (event.event_type === 'conference_invite') {
                        const data = JSON.parse(event.data);
                        showConferenceInviteDialog(data);
                    } else if (event.event_type === 'conference_message') {
                        const data = JSON.parse(event.data);
                        // 使用 conference_number（与后端字段名一致）
                        if (data.conference_number == currentChatId && currentChatType === 'conference') {
                            appendMessage(data.message, 'other', 'Peer ' + data.peer_number);
                        }
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
        msgDiv.innerHTML = `<div class="sender">${t('friend_label', senderId)}</div>${escapeHtml(text)}`;
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

// Show conference invite dialog (同意-左, 拒绝-中, 忽略-右)
function showConferenceInviteDialog(data) {
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:1000;display:flex;align-items:center;justify-content:center;';

    const dialog = document.createElement('div');
    dialog.style.cssText = 'background:#21262d;padding:20px;border-radius:8px;max-width:400px;width:90%;color:#c9d1d9;box-shadow:0 4px 12px rgba(0,0,0,0.5);';

    dialog.innerHTML = `
        <h3 style="margin-top:0;color:#58a6ff;">${t('conference.invitation_received')}</h3>
        <p style="margin:10px 0;">${t('conference.invitation_from', data.friend_number)} ${t('conference.invite_message')}</p>
        <div style="text-align:right;margin-top:20px;">
            <button id="acceptBtn" style="margin-right:10px;padding:8px 16px;background:#238636;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('conference.accept')}</button>
            <button id="rejectBtn" style="margin-right:10px;padding:8px 16px;background:#f85149;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('conference.reject')}</button>
            <button id="ignoreBtn" style="padding:8px 16px;background:#484f58;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('conference.ignore')}</button>
        </div>
    `;

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    // 同意按钮（最左）
    document.getElementById('acceptBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/conferences/join', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `friend_number=${data.friend_number}&cookie=${data.cookie}`
        }).then(r => r.json())
          .then(data => {
              alert(t('conference_joined', data.conference_id));
              loadContacts('conferences');
          }).catch(err => {
              alert(t('conference_join_failed') + ': ' + err);
          });
    };

    // 拒绝按钮（中间）
    document.getElementById('rejectBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/conferences/reject', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `friend_number=${data.friend_number}`
        }).then(() => {
            console.log('Conference invite rejected');
        });
    };

    // 忽略按钮（最右）
    document.getElementById('ignoreBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/conferences/ignore', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `friend_number=${data.friend_number}`
        }).then(() => {
            console.log('Conference invite ignored');
        });
    };
}

// Send message
function sendMessage() {
    console.log('sendMessage called: currentChatId=' + currentChatId + ', type=' + typeof currentChatId + ', currentChatType=' + currentChatType);
    if (currentChatId === null || currentChatId === undefined || !currentChatType) {
        alert(t('select_chat_first'));
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
    }).then(r => {
        if (!r.ok) {
            return r.json().then(err => { throw err; });
        }
        appendMessage(msg, 'self');
        input.value = '';
    }).catch(err => {
        console.error('Send error:', err);
        alert(t('send_failed') + ': ' + (err.error || JSON.stringify(err)));
    });
}

// Add friend
function addFriend() {
    const input = document.getElementById('addFriendInput');
    const pubkey = input.value.trim();
    if (!pubkey || (pubkey.length !== 64 && pubkey.length !== 76)) {
        alert(t('add_friend_prompt'));
        return;
    }
    fetch('/api/friends', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `public_key=${encodeURIComponent(pubkey)}`
    }).then(r => r.json())
      .then(data => {
          alert(t('add_friend_success'));
          input.value = '';
          loadContacts('friends');
      }).catch(err => {
          alert(t('add_friend_failed') + ': ' + err);
      });
}

// Create group
function createGroup() {
    fetch('/api/groups', {
        method: 'POST'
    }).then(r => r.json())
      .then(data => {
          alert(t('group_created'));
          loadContacts('groups');
      }).catch(err => {
          alert(t('group_create_failed') + ': ' + err);
      });
}

// Create conference
function createConference() {
    fetch('/api/conferences', {
        method: 'POST'
    }).then(r => r.json())
      .then(data => {
          alert(t('conference_created'));
          loadContacts('conferences');
      }).catch(err => {
          alert(t('conference_create_failed') + ': ' + err);
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
          document.getElementById('infoFriendName').textContent = data.name || t('no_name_label');
          document.getElementById('infoFriendId').textContent = data.friend_id;
          document.getElementById('infoFriendStatus').textContent = data.status || t('unknown');
          document.getElementById('infoFriendConn').textContent = data.connection_status || t('unknown');
          document.getElementById('infoFriendPk').textContent = data.public_key || t('unknown');
          
          document.getElementById('friendInfoModal').classList.remove('hidden');
      });
    hideFriendMenu();
}

function hideFriendInfo() {
    document.getElementById('friendInfoModal').classList.add('hidden');
}

function deleteFriend() {
    if (!selectedFriendId) return;
    
    if (!confirm(t('confirm_delete_friend', selectedFriendId))) {
        return;
    }
    
    fetch('/api/friend_delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${selectedFriendId}`
    }).then(r => r.json())
      .then(data => {
          alert(t('friend_deleted'));
          loadContacts('friends');
      }).catch(err => {
          alert(t('delete_failed') + ': ' + err);
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
            alert(t('save_failed') + ': ' + err);
        });
}

// Bootstrap
function bootstrap() {
    fetch('/api/bootstrap', {
        method: 'POST'
    }).then(() => {
        alert(t('connecting_network'));
        setTimeout(loadSelfInfo, 2000);
    }).catch(err => {
        alert(t('connect_failed') + ': ' + err);
    });
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM loaded, initializing...');
    initLanguage();
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
window.switchLanguage = switchLanguage;
