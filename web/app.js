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

// 使用 getter/setter 监控 currentFilter 的变化
let _currentFilter = 'all';
Object.defineProperty(window, 'currentFilter', {
    get: function() { return _currentFilter; },
    set: function(newVal) {
        console.trace('currentFilter CHANGED from', _currentFilter, 'to', newVal);
        _currentFilter = newVal;
    }
});

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
    const friendMenuItems = document.querySelectorAll('#friendMenu .menu-item');
    if (friendMenuItems[0]) friendMenuItems[0].textContent = t('context_menu.view_info');
    if (friendMenuItems[1]) friendMenuItems[1].textContent = t('context_menu.invite_to_conference');
    if (friendMenuItems[2]) friendMenuItems[2].textContent = t('context_menu.invite_to_group');
    if (friendMenuItems[3]) friendMenuItems[3].textContent = t('context_menu.delete_friend');

    const conferenceMenuItems = document.querySelectorAll('#conferenceMenu .menu-item');
    if (conferenceMenuItems[0]) conferenceMenuItems[0].textContent = t('context_menu.view_info');
    if (conferenceMenuItems[1]) conferenceMenuItems[1].textContent = t('context_menu.view_members');
    if (conferenceMenuItems[2]) conferenceMenuItems[2].textContent = t('context_menu.leave_conference');
    
    const groupMenuItems = document.querySelectorAll('#groupMenu .menu-item');
    if (groupMenuItems[0]) groupMenuItems[0].textContent = t('context_menu.view_info');
    if (groupMenuItems[1]) groupMenuItems[1].textContent = t('context_menu.view_members');
    if (groupMenuItems[2]) groupMenuItems[2].textContent = t('context_menu.leave_group');
    
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
    
    // 会议信息模态框
    const conferenceTitle = document.getElementById('conferenceInfoTitle');
    if (conferenceTitle) conferenceTitle.textContent = t('conference_info_title');
    
    // 群组信息模态框
    const groupTitle = document.getElementById('groupInfoTitle');
    if (groupTitle) groupTitle.textContent = t('group_info_title');
    
    // 选择会议对话框
    const selectTitle = document.getElementById('selectConferenceTitle');
    if (selectTitle) selectTitle.textContent = t('select_conference');
    
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
            let errorMsg = t('load_contacts_failed');
            if (err && err.message) {
                errorMsg += ': ' + err.message;
            }
            document.getElementById('contactList').innerHTML = 
                '<div style="padding:10px;color:#f85149;">' + errorMsg + '</div>';
        });
}

// Render merged contact list with emoji indicators
function renderContactList(filter) {
    console.log('renderContactList: filter=' + filter + ', currentFilter=' + currentFilter + ', currentChatId=' + currentChatId + ', currentChatType=' + currentChatType);
    const list = document.getElementById('contactList');
    let html = '';
    
    // 防御性检查：确保数组存在
    if (!Array.isArray(contacts.friends)) contacts.friends = [];
    if (!Array.isArray(contacts.groups)) contacts.groups = [];
    if (!Array.isArray(contacts.conferences)) contacts.conferences = [];
    
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
            // g is object: {group_number, group_name, chat_id, ...}
            const groupId = g.group_number;
            // 名称为空时：显示 "number - chat_id前7位"
            let groupName = g.group_name;
            if (!groupName) {
                const shortId = (g.chat_id || '').substring(0, 7);
                groupName = shortId ? `${groupId} - ${shortId}` : `${t('group')} ${groupId}`;
            }
            const isSelected = groupId == currentChatId && currentChatType === 'group';
            const emoji = '👥';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-group-id="${groupId}" onclick="selectContact(${groupId}, 'group')">
                    <span class="${g.is_connected ? 'online-dot' : 'offline-dot'}"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">${groupName}</span>
                </div>
            `;
        });
    }
    
    // Add conferences
    if (filter === 'all' || filter === 'conferences') {
        contacts.conferences.forEach(c => {
            // c is object: {conference_number, conference_name, chat_id, ...}
            const confId = c.conference_number;
            // 名称为空时：显示 "number - chat_id前7位"
            let confName = c.conference_name;
            if (!confName) {
                const shortId = (c.chat_id || '').substring(0, 7);
                confName = shortId ? `${confId} - ${shortId}` : `${t('conference_item')} ${confId}`;
            }
            const isSelected = confId == currentChatId && currentChatType === 'conference';
            const emoji = '🎙';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-conference-id="${confId}" onclick="selectContact(${confId}, 'conference')">
                    <span class="${c.is_connected ? 'online-dot' : 'offline-dot'}"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">${confName}</span>
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
    console.trace('showTab TRACE');
    console.log('showTab called: tab=' + tab + ', old currentFilter=' + currentFilter);
    // 注意：不要设置 currentChatType = tab，因为 tab 是复数形式（groups/conferences）
    // 而 currentChatType 应该是单数形式（group/conference）
    currentFilter = tab;
    console.log('showTab after set: currentFilter=' + currentFilter);
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    event.target.classList.add('active');

    // 控制底部区域显示
    const friendBottom = document.querySelector('.add-friend-bottom');
    const groupBottom = document.querySelector('.join-group-bottom');
    
    if (friendBottom) friendBottom.style.display = (tab === 'friends' || tab === 'all') ? 'flex' : 'none';
    if (groupBottom) groupBottom.style.display = (tab === 'groups' || tab === 'all') ? 'flex' : 'none';

    loadContacts(tab);
}

// Select a contact (friend/group/conference)
let lastSelectTime = 0;
function selectContact(id, type) {
    const now = Date.now();
    if (now - lastSelectTime < 300) {
        console.warn('selectContact: ignoring rapid re-call (possible event loop)');
        return;
    }
    lastSelectTime = now;
    
    console.trace('selectContact TRACE');
    console.log('selectContact called: id=' + id + ', type=' + type + ', id type=' + typeof id + ', currentFilter=' + currentFilter);
    
    // 防御：如果 type 是复数形式，修正为单数
    if (type === 'groups' || type === 'conferences') {
        console.error('selectContact: invalid type=' + type + ', correcting to singular');
        type = type.endsWith('s') ? type.slice(0, -1) : type;
    }
    
    currentChatId = id;
    currentChatType = type;
    console.log('After set: currentChatId=' + currentChatId + ', currentChatType=' + currentChatType + ', currentFilter=' + currentFilter);
    
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
    
    // 只更新选中状态，不重新渲染整个列表
    updateSelection(id, type);
}

// 只更新选中状态，不重新渲染整个列表
function updateSelection(id, type) {
    console.log('updateSelection: id=' + id + ', type=' + type + ', currentFilter=' + currentFilter);
    
    // 移除所有选中状态
    document.querySelectorAll('.list-item').forEach(item => {
        item.classList.remove('selected');
    });
    
    // 给当前选中的条目添加选中状态
    let selector = '';
    if (type === 'friend') {
        selector = `.list-item[data-friend-id="${id}"]`;
    } else if (type === 'group') {
        selector = `.list-item[data-group-id="${id}"]`;
    } else if (type === 'conference') {
        selector = `.list-item[data-conference-id="${id}"]`;
    }
    
    if (selector) {
        const selectedItem = document.querySelector(selector);
        if (selectedItem) {
            selectedItem.classList.add('selected');
        } else {
            console.warn('updateSelection: item not found with selector=' + selector);
        }
    }
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
                        // Friend info updated, refresh contacts with current filter
                        loadContacts(currentFilter);
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
                    } else if (event.event_type === 'group_invite') {
                        const data = JSON.parse(event.data);
                        showGroupInviteDialog(data);
                    } else if (event.event_type === 'group_message') {
                        const data = JSON.parse(event.data);
                        if (data.group_number == currentChatId && currentChatType === 'group') {
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

// Show group invite dialog (同意-左, 拒绝-中, 忽略-右)
function showGroupInviteDialog(data) {
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:1000;display:flex;align-items:center;justify-content:center;';

    const dialog = document.createElement('div');
    dialog.style.cssText = 'background:#21262d;padding:20px;border-radius:8px;max-width:400px;width:90%;color:#c9d1d9;box-shadow:0 4px 12px rgba(0,0,0,0.5);';

    dialog.innerHTML = `
        <h3 style="margin-top:0;color:#58a6ff;">${t('group.invitation_received')}</h3>
        <p style="margin:10px 0;">${t('group.invitation_from', data.friend_number)} ${t('group.invite_message')}</p>
        <div style="text-align:right;margin-top:20px;">
            <button id="groupAcceptBtn" style="margin-right:10px;padding:8px 16px;background:#238636;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('group.accept')}</button>
            <button id="groupRejectBtn" style="margin-right:10px;padding:8px 16px;background:#f85149;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('group.reject')}</button>
            <button id="groupIgnoreBtn" style="padding:8px 16px;background:#484f58;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;">${t('group.ignore')}</button>
        </div>
    `;

    overlay.appendChild(dialog);
    document.body.appendChild(overlay);

    // 同意按钮（最左）
    document.getElementById('groupAcceptBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/groups/accept', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `invite_data=${data.invite_data}&friend_number=${data.friend_number}&name=${encodeURIComponent(data.name || '')}`
        }).then(r => r.json())
          .then(data => {
              if (data.error) {
                  alert(t('group.join_failed') + ': ' + data.error);
              } else {
                  alert(t('group.joined', data.group_number));
                  loadContacts('groups');
              }
          }).catch(err => {
              alert(t('group.join_failed') + ': ' + err);
          });
    };

    // 拒绝按钮（中间）
    document.getElementById('groupRejectBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/groups/reject', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `friend_number=${data.friend_number}`
        }).then(() => {
            console.log('Group invite rejected');
        });
    };

    // 忽略按钮（最右）
    document.getElementById('groupIgnoreBtn').onclick = function() {
        document.body.removeChild(overlay);
        fetch('/api/groups/ignore', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `friend_number=${data.friend_number}`
        }).then(() => {
            console.log('Group invite ignored');
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

// Add friend from bottom input
function addFriendFromBottom() {
    const input = document.getElementById('addFriendInputBottom');
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

// Join group from bottom input
function joinGroupFromBottom() {
    const chatId = document.getElementById('joinGroupInputBottom').value.trim();

    if (!chatId) {
        alert(t('please_enter_chat_id') || '请输入群组 chat_id');
        return;
    }

    fetch('/api/groups/join', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `chat_id=${encodeURIComponent(chatId)}`
    }).then(r => r.json())
      .then(data => {
          if (data.error) {
              alert(t('group.join_failed') + ': ' + data.error);
          } else {
              alert(t('group.joined') + ' (Group #' + data.group_number + ')');
              document.getElementById('joinGroupInputBottom').value = '';
              loadContacts('groups');
          }
      }).catch(err => {
          alert(t('group.join_failed') + ': ' + err);
      });
}

// Create group
function createGroup() {
    // Show group creation modal
    document.getElementById('groupModal').classList.remove('hidden');
    document.getElementById('groupNameInput').value = '';
    document.getElementById('groupCreatorNameInput').value = '';
    document.getElementById('groupPasswordInput').value = '';
    document.getElementById('groupPrivacySelect').value = 'public';
    document.getElementById('groupNameInput').focus();
}

function closeGroupModal() {
    document.getElementById('groupModal').classList.add('hidden');
}

function confirmCreateGroup() {
    const groupName = document.getElementById('groupNameInput').value.trim();
    const creatorName = document.getElementById('groupCreatorNameInput').value.trim();
    const password = document.getElementById('groupPasswordInput').value;
    const privacyState = document.getElementById('groupPrivacySelect').value;
    
    if (!groupName) {
        alert(t('modals.labels.group_name') + ' ' + t('cannot_be_empty'));
        return;
    }
    if (!creatorName) {
        alert(t('modals.labels.creator_name') + ' ' + t('cannot_be_empty'));
        return;
    }
    
    const params = new URLSearchParams();
    params.append('group_name', groupName);
    params.append('name', creatorName);
    if (password) params.append('password', password);
    if (privacyState === 'private') params.append('privacy_state', 'private');
    
    fetch('/api/groups', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
    }).then(r => r.json())
      .then(data => {
          if (data.error) {
              alert(t('group_create_failed') + ': ' + data.error);
          } else {
              alert(t('group_created') + ' (Group #' + data.group_number + ')');
              closeGroupModal();
              loadContacts('groups');
          }
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

// Leave group
function leaveGroup() {
    if (!selectedGroupId) return;
    if (!confirm(t('confirm_leave_group', selectedGroupId))) {
        return;
    }
    fetch('/api/groups/leave', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `group_number=${selectedGroupId}`
    }).then(r => r.json())
      .then(data => {
          if (data.error) {
              alert(t('group_leave_failed') + ': ' + data.error);
          } else {
              alert(t('group_leave_success'));
              if (currentChatId == selectedGroupId && currentChatType === 'group') {
                  currentChatId = null;
                  currentChatType = null;
                  document.getElementById('chatHeaderText').textContent = t('select_chat_object');
                  document.getElementById('messageArea').innerHTML = '';
              }
              loadContacts('groups');
          }
      }).catch(err => {
          alert(t('group_leave_failed') + ': ' + err);
      });
}

// Context menu variables
let selectedFriendId = null;
let selectedConferenceId = null;
let selectedGroupId = null;
let inviteFriendId = null;

// Use event delegation for contact list context menu
document.addEventListener('DOMContentLoaded', () => {
    const contactList = document.getElementById('contactList');
    if (contactList) {
        contactList.addEventListener('contextmenu', (event) => {
            const item = event.target.closest('.list-item');
            if (!item) return;
            
            event.preventDefault();
            
            // Determine type and show appropriate menu
            if (item.dataset.friendId) {
                selectedFriendId = item.dataset.friendId;
                showContextMenu('friendMenu', event.pageX, event.pageY);
            } else if (item.dataset.conferenceId) {
                selectedConferenceId = item.dataset.conferenceId;
                showContextMenu('conferenceMenu', event.pageX, event.pageY);
            } else if (item.dataset.groupId) {
                selectedGroupId = item.dataset.groupId;
                showContextMenu('groupMenu', event.pageX, event.pageY);
            }
        });
    }

    // Handle friend menu item clicks
    const friendMenu = document.getElementById('friendMenu');
    if (friendMenu) {
        friendMenu.addEventListener('click', (event) => {
            const action = event.target.getAttribute('data-action');
            if (action === 'info') {
                window.showFriendInfo(selectedFriendId);
            } else if (action === 'invite_conference') {
                window.inviteToConference(selectedFriendId);
            } else if (action === 'invite_group') {
                window.inviteToGroup(selectedFriendId);
            } else if (action === 'delete') {
                window.deleteFriend();
            }
        });
    }

    // Handle conference menu item clicks
    const conferenceMenu = document.getElementById('conferenceMenu');
    if (conferenceMenu) {
        conferenceMenu.addEventListener('click', (event) => {
            const action = event.target.getAttribute('data-action');
            if (action === 'info') {
                window.showConferenceInfo(selectedConferenceId);
            } else if (action === 'members') {
                window.showConferenceMembers(selectedConferenceId);
            } else if (action === 'leave') {
                window.leaveConference();
            }
        });
    }

    // Handle group menu item clicks
    const groupMenu = document.getElementById('groupMenu');
    if (groupMenu) {
        groupMenu.addEventListener('click', (event) => {
            const action = event.target.getAttribute('data-action');
            if (action === 'info') {
                window.showGroupInfo(selectedGroupId);
            } else if (action === 'members') {
                window.showGroupMembers(selectedGroupId);
            } else if (action === 'leave') {
                window.leaveGroup();
            }
        });
    }

    hideAllContextMenus();
});

function showContextMenu(menuId, pageX, pageY) {
    hideAllContextMenus();
    const menu = document.getElementById(menuId);
    if (menu) {
        menu.style.display = 'block';
        menu.style.left = pageX + 'px';
        menu.style.top = pageY + 'px';
        
        // Hide menu when clicking elsewhere
        setTimeout(() => {
            document.addEventListener('click', hideAllContextMenus, { once: true });
        }, 0);
    }
}

function hideAllContextMenus() {
    ['friendMenu', 'conferenceMenu', 'groupMenu'].forEach(id => {
        const menu = document.getElementById(id);
        if (menu) menu.style.display = 'none';
    });
}

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
    
    hideAllContextMenus();
}

// Conference functions
function showConferenceInfo(conferenceId) {
    document.getElementById('infoConferenceId').textContent = conferenceId;
    document.getElementById('infoConferenceType').textContent = 'Tox Conference';
    document.getElementById('conferenceInfoModal').classList.remove('hidden');
    hideAllContextMenus();
}

function hideConferenceInfo() {
    document.getElementById('conferenceInfoModal').classList.add('hidden');
}

function leaveConference() {
    if (!selectedConferenceId) return;
    
    if (!confirm(t('confirm_leave_conference', selectedConferenceId))) {
        return;
    }
    
    fetch('/api/conference_delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `conference_id=${selectedConferenceId}`
    }).then(r => r.json())
      .then(data => {
          alert(t('conference_leave_success'));
          loadContacts('conferences');
      }).catch(err => {
          alert(t('conference_leave_failed') + ': ' + err);
      });
    
    hideAllContextMenus();
}

// Group functions
function showGroupInfo(groupId) {
    document.getElementById('infoGroupId').textContent = groupId;
    document.getElementById('groupInfoModal').classList.remove('hidden');
    hideAllContextMenus();
}

function hideGroupInfo() {
    document.getElementById('groupInfoModal').classList.add('hidden');
}

// Member list functions
async function fetchConferenceMembers(conferenceId) {
    try {
        const resp = await fetch(`/api/conference/members?conference_id=${conferenceId}`);
        const data = await resp.json();
        return data.members || [];
    } catch (err) {
        console.error('Failed to fetch conference members:', err);
        return [];
    }
}

async function fetchGroupMembers(groupId) {
    try {
        const resp = await fetch(`/api/group/members?group_number=${groupId}`);
        const data = await resp.json();
        return data.members || [];
    } catch (err) {
        console.error('Failed to fetch group members:', err);
        return [];
    }
}

async function showConferenceMembers(conferenceId) {
    const members = await fetchConferenceMembers(conferenceId);
    const title = t('member_list.title.conference').replace('{0}', conferenceId);
    document.getElementById('memberListTitle').textContent = title;
    
    let html = '';
    if (members.length === 0) {
        html = '<div style="text-align: center; color: #8b949e;">暂无成员</div>';
    } else {
        members.forEach(m => {
            html += `<div style="padding: 8px; border-bottom: 1px solid #30363d;">
                        <span style="color: #00d4aa;">Peer ${m.peer_number}</span>: 
                        <span style="color: #c9d1d9;">${m.name}</span>
                      </div>`;
        });
    }
    
    document.getElementById('memberListContent').innerHTML = html;
    document.getElementById('memberListModal').classList.remove('hidden');
}

async function showGroupMembers(groupId) {
    const members = await fetchGroupMembers(groupId);
    const title = t('member_list.title.group').replace('{0}', groupId);
    document.getElementById('memberListTitle').textContent = title;
    
    let html = '';
    if (members.length === 0) {
        html = '<div style="text-align: center; color: #8b949e;">暂无成员</div>';
    } else {
        members.forEach(m => {
            html += `<div style="padding: 8px; border-bottom: 1px solid #30363d;">
                        <span style="color: #00d4aa;">Peer ${m.peer_number}</span>: 
                        <span style="color: #c9d1d9;">${m.name}</span>
                      </div>`;
        });
    }
    
    document.getElementById('memberListContent').innerHTML = html;
    document.getElementById('memberListModal').classList.remove('hidden');
}

function hideMemberList() {
    document.getElementById('memberListModal').classList.add('hidden');
}

// Invite functions
function inviteToConference(friendId) {
    inviteFriendId = friendId;
    
    fetch('/api/conferences')
        .then(r => r.json())
        .then(data => {
            const select = document.getElementById('conferenceSelect');
            select.innerHTML = '';
            data.conferences.forEach(c => {
                const option = document.createElement('option');
                option.value = c;
                option.textContent = t('conference_item') + ' ' + c;
                select.appendChild(option);
            });
            document.getElementById('selectConferenceModal').classList.remove('hidden');
        })
        .catch(err => {
            alert(t('load_failed') + ': ' + err);
        });
    
    hideAllContextMenus();
}

function confirmConferenceInvite() {
    const confId = document.getElementById('conferenceSelect').value;
    fetch('/api/conference_invite', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${inviteFriendId}&conference_id=${confId}`
    }).then(r => r.json())
      .then(data => {
          if (data.error) {
              alert(t('invite_failed') + ': ' + data.error);
          } else {
              alert(t('invite_success'));
              hideSelectConference();
          }
      }).catch(err => {
          alert(t('invite_failed') + ': ' + err);
      });
}

function confirmGroupInvite() {
    const groupId = document.getElementById('groupSelect').value;
    fetch('/api/groups/invite', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `friend_id=${inviteFriendId}&group_id=${groupId}`
    }).then(r => r.json())
      .then(data => {
          if (data.error) {
              alert(t('invite_failed') + ': ' + data.error);
          } else {
              alert(t('invite_success'));
              hideSelectGroup();
          }
      }).catch(err => {
          alert(t('invite_failed') + ': ' + err);
      });
}

function hideSelectConference() {
    document.getElementById('selectConferenceModal').classList.add('hidden');
}

// Invite friend to group
function inviteToGroup(friendId) {
    // 获取群组列表
    fetch('/api/groups')
        .then(r => r.json())
        .then(data => {
            if (!data.groups || data.groups.length === 0) {
                alert(t('no_group') || '没有可用的群组');
                return;
            }
            
            // 创建选择对话框
            const select = document.createElement('select');
            data.groups.forEach(g => {
                const option = document.createElement('option');
                option.value = g;
                option.textContent = t('group') + ' ' + g;
                select.appendChild(option);
            });
            
            const groupId = prompt('选择群组 ID:', data.groups[0]);
            if (groupId) {
                fetch('/api/groups/invite', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `friend_id=${friendId}&group_id=${groupId}`
                }).then(() => {
                    alert(t('invite_success') || '邀请成功');
                }).catch(err => {
                    alert(t('invite_failed') + ': ' + err);
                });
            }
        })
        .catch(err => {
            alert(t('load_failed') + ': ' + err);
        });
    
    hideAllContextMenus();
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
        hideAllContextMenus();
        hideEditSelf();
        hideConferenceInfo();
        hideGroupInfo();
        hideSelectConference();
        hideMemberList();
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
window.leaveConference = leaveConference;
window.showConferenceInfo = showConferenceInfo;
window.hideConferenceInfo = hideConferenceInfo;
window.showGroupInfo = showGroupInfo;
window.hideGroupInfo = hideGroupInfo;
window.inviteToConference = inviteToConference;
window.confirmConferenceInvite = confirmConferenceInvite;
window.confirmGroupInvite = confirmGroupInvite;
window.hideSelectConference = hideSelectConference;
window.switchLanguage = switchLanguage;
window.joinGroupFromBottom = joinGroupFromBottom;
window.inviteToGroup = inviteToGroup;
window.showConferenceMembers = showConferenceMembers;
window.showGroupMembers = showGroupMembers;
window.hideMemberList = hideMemberList;
