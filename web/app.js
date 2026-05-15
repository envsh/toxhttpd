let currentChatId = null;
let currentChatType = null; // 'friend', 'group', or 'conference'
let lastEventId = 0;
let currentEventId = 0; // For deleting events after processing
let pollTimeout = null;
let selfAddress = ''; // 保存完整地址

// Contact list data
let contacts = {
    friends: [],
    groups: [],
    conferences: []
};
let contactsLoading = false;
let contactsReloadPending = false;

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
    const sendBtn = document.querySelector('.send-btn');
    if (sendBtn) sendBtn.textContent = t('buttons.send');
    
    // 更新聊天头部（使用 updateChatHeader）
    if (currentChatId && currentChatType) {
        updateChatHeader(currentChatId, currentChatType);
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
    if (groupMenuItems[2]) groupMenuItems[2].textContent = t('context_menu.rename_nick');
    if (groupMenuItems[3]) groupMenuItems[3].textContent = t('context_menu.leave_group');
    
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

    // 群组重命名模态框
    const renameTitle = document.getElementById('groupRenameTitle');
    if (renameTitle) renameTitle.textContent = t('rename.title');
    const renameCurrent = document.getElementById('groupRenameCurrent');
    if (renameCurrent) renameCurrent.textContent = t('rename.current_nick') + ': --';
    const renameOptSelf = document.getElementById('renameOptSelf');
    if (renameOptSelf) renameOptSelf.textContent = t('rename.use_self_nick');
    const renameOptCustom = document.getElementById('renameOptCustom');
    if (renameOptCustom) renameOptCustom.textContent = t('rename.custom_nick');
    const renameOptRandom = document.getElementById('renameOptRandom');
    if (renameOptRandom) renameOptRandom.textContent = t('rename.random_nick');
    const renameConfirmBtn = document.getElementById('renameConfirmBtn');
    if (renameConfirmBtn) renameConfirmBtn.textContent = t('rename.confirm');
    const renameCancelBtns = document.querySelectorAll('#groupRenameModal .modal-actions button');
    if (renameCancelBtns[1]) renameCancelBtns[1].textContent = t('buttons.cancel');
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
    if (contactsLoading) {
        contactsReloadPending = true;
        return;
    }
    contactsLoading = true;
    
    // Load friends
    fetch('/api/friends')
        .then(r => r.json())
        .then(data => {
            const ids = data.friends || [];
            contacts.friends = ids;
            // Batch 3 per request, parallel
            const chunkSize = 3;
            const chunks = [];
            for (let i = 0; i < ids.length; i += chunkSize)
                chunks.push(ids.slice(i, i + chunkSize));

            return Promise.all(chunks.map(chunk =>
                fetch('/api/friend', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `friend_ids=${chunk.join(',')}`
                }).then(r => r.json())
            )).then(results => {
                const all = results.flat();
                contacts.friends = all.filter(f => !f.error);
                return all;
            });
        })
        .then(friendDetails => {
            console.log('Friend details loaded:', friendDetails);
            // 填充 peerInfoMap 好友条目
            friendDetails.forEach(f => {
                if (f.error) return;
                peerInfoMap["friend_" + f.friendId] = {
                    name: f.name || '',
                    peerNumber: f.friendId,
                    status: f.status || 0,
                    statusStr: f.statusStr || 'none',
                    statusText: f.statusText || '',
                    iconUrl: f.iconUrl || '',
                    publicKey: f.publicKey || '',
                    isSelf: false
                };
            });
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
        })
        .then(() => {
            contactsLoading = false;
            if (contactsReloadPending) {
                contactsReloadPending = false;
                loadContacts(currentFilter);
            }
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
            const isSelected = f.friendId == currentChatId && currentChatType === 'friend';
            const dotClass = f.status ? 'online-dot' : 'offline-dot';
            const emoji = '👤';
            // Show name, or public key first 7 chars if name empty
            let displayName = f.name;
            if (!displayName || displayName === '') {
                displayName = (f.publicKey || '').substring(0, 7) + '...';
            }
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-friend-id="${f.friendId}" onclick="selectContact(${f.friendId}, 'friend')">
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
            const groupId = g.groupNumber;
            // 名称为空时：显示 "number - chatId前7位"
            let groupName = g.groupName;
            if (!groupName) {
                const shortId = (g.chatId || '').substring(0, 7);
                groupName = shortId ? `${groupId} - ${shortId}` : `${t('group')} ${groupId}`;
            }
            const isSelected = groupId == currentChatId && currentChatType === 'group';
            const emoji = '👥';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-group-id="${groupId}" onclick="selectContact(${groupId}, 'group')">
                    <span class="${g.isConnected ? 'online-dot' : 'offline-dot'}"></span>
                    <span class="item-emoji">${emoji}</span>
                    <span class="item-text">${groupName}</span>
                </div>
            `;
        });
    }
    
    // Add conferences
    if (filter === 'all' || filter === 'conferences') {
        contacts.conferences.forEach(c => {
            const confId = c.conferenceNumber;
            // 名称为空时：显示 "number - chatId前7位"
            let confName = c.conferenceName;
            if (!confName) {
                const shortId = (c.chatId || '').substring(0, 7);
                confName = shortId ? `${confId} - ${shortId}` : `${t('conference_item')} ${confId}`;
            }
            const isSelected = confId == currentChatId && currentChatType === 'conference';
            const emoji = '🎙';
            html += `
                <div class="list-item ${isSelected ? 'selected' : ''}" data-conference-id="${confId}" onclick="selectContact(${confId}, 'conference')">
                    <span class="${c.isConnected ? 'online-dot' : 'offline-dot'}"></span>
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
    
    updateChatHeader(id, type);
    vsc.clear();

    // 加载历史消息
    loadMessageHistory();

    // 只更新选中状态，不重新渲染整个列表
    updateSelection(id, type);

    // 预加载成员列表到 peerInfoMap 缓存
    if (type === 'conference') {
        fetchConferenceMembers(id).then(members => {
            members.forEach(m => {
                peerInfoMap[`conference_${id}_${m.peer_number}`] = {
                    name: m.name, peerNumber: m.peer_number
                };
            });
        }).catch(() => {});
    } else if (type === 'group') {
        fetchGroupMembers(id).then(result => {
            const members = result.members;
            members.forEach(m => {
                const key = `group_${id}_${m.peerNumber}`;
                peerInfoMap[key] = {
                    name: m.name, peerNumber: m.peerNumber,
                    status: m.status,
                    statusStr: m.statusStr || '',
                    statusText: m.statusText || '',
                    iconUrl: m.iconUrl || '',
                    role: m.role, roleStr: m.roleStr || '',
                    publicKey: m.publicKey, isSelf: m.isSelf,
                    peerIp: m.peerIp || ''
                };
            });
        }).catch(() => {});
    }
}

// ── Link detection ──
function linkifyText(text) {
    if (!text) return '';
    var escaped = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    return escaped.replace(/(https?:\/\/[^\s<>&"']+)/g, '<a href="$1" target="_blank" rel="noopener noreferrer">$1</a>');
}

// ── Translation ──
const TARGET_LANG = 'zh-CN';

function translateMessage(dataIdx) {
    const msg = vsc.data[dataIdx];
    if (!msg) return;
    if (msg.translationInProgress) return;

    // Toggle if already translated
    if (msg.translatedText && msg.translatedText.length > 0) {
        msg.showTranslation = !msg.showTranslation;
        updateMessageNode(dataIdx);
        return;
    }

    msg.translateError = '';
    msg.translationInProgress = true;
    updateMessageNode(dataIdx);

    fetch('/api/translate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text: msg.text, to: TARGET_LANG })
    })
    .then(r => r.json().then(data => ({ ok: r.ok, data })))
    .then(({ ok, data }) => {
        if (ok && data.translated_text) {
            msg.translatedText = data.translated_text;
            msg.showTranslation = true;
            msg.translateError = '';
        } else {
            msg.translateError = data.error || '翻译失败';
        }
        msg.translationInProgress = false;
        updateMessageNode(dataIdx);
    })
    .catch(() => {
        msg.translateError = '网络连接失败';
        msg.translationInProgress = false;
        updateMessageNode(dataIdx);
    });
}

function updateMessageNode(dataIdx) {
    const pi = vsc.usedSlots.get(dataIdx);
    if (pi === undefined) return;
    const node = vsc.pool[pi].node;
    vsc._updateNode(node, vsc.data[dataIdx], vsc.cumulative[dataIdx], dataIdx);
}

// ── Chat header update ──
function updateChatHeader(id, type) {
    var badge = document.getElementById('chatTypeBadge');
    var nameEl = document.getElementById('chatContactName');
    var dot = document.getElementById('headerStatusDot');
    if (!id) {
        badge.textContent = '';
        nameEl.textContent = '';
        dot.className = 'online-dot hidden';
        return;
    }
    var displayName = '';
    var isOnline = false;
    if (type === 'friend') {
        badge.textContent = '👤';
        var found = contacts.friends.find(function(f) { return f.friendId == id; });
        if (found) {
            displayName = found.name || found.publicKey || id;
            isOnline = found.status && found.status !== 0;
        } else {
            displayName = id;
        }
    } else if (type === 'group') {
        badge.textContent = '👥';
        var found = contacts.groups.find(function(g) { return g.groupNumber == id; });
        displayName = found ? (found.groupName || id) : id;
        isOnline = found ? found.isConnected : false;
    } else if (type === 'conference') {
        badge.textContent = '🎙';
        var found = contacts.conferences.find(function(c) { return c.conferenceNumber == id; });
        displayName = found ? (found.conferenceName || id) : id;
        isOnline = found ? found.isConnected : false;
    }
    nameEl.textContent = displayName;
    var dotClass = 'online-dot';
    if (!isOnline) dotClass = 'online-dot offline';
    dot.className = dotClass;
}

// Load message history for current chat
function loadMessageHistory() {
    if (!currentChatId || !currentChatType) {
        console.warn('loadMessageHistory: no chat selected');
        return;
    }

    const contactId = currentChatId;
    const contactType = currentChatType;

    fetch(`/api/messages/history?contact_id=${contactId}&contact_type=${contactType}`)
        .then(r => r.json())
        .then(data => {
            if (!data.messages || data.messages.length === 0) {
                console.log('No message history found');
                return;
            }

            const batch = [];
            data.messages.forEach(msg => {
                const selfPubkey = selfAddress ? selfAddress.toUpperCase().substring(0, 64) : '';
                const isSelf = msg.sender_pubkey.toUpperCase() === selfPubkey;

                let senderName = 'Me';
                let peerNumber = -1;
                let avatarText = 'M';
                let ipAddress = '';
                if (!isSelf) {
                    if (contactType === 'friend') {
                        var fe = peerInfoMap["friend_" + contactId];
                        senderName = (fe && fe.name) || '';
                        peerNumber = contactId;
                        avatarText = senderName ? senderName.charAt(0).toUpperCase() : 'F';
                    } else {
                        const peerKey = `${contactType}_${contactId}_${msg.sender_number}`;
                        const cached = peerInfoMap[peerKey];
                        senderName = (cached && cached.name) || '';
                        peerNumber = msg.sender_number;
                        avatarText = senderName ? senderName.charAt(0).toUpperCase() : 'P';
                        ipAddress = (cached && cached.peerIp) || '';
                    }
                }

                const msgDate = new Date(msg.created_at);
                const timestamp = msgDate.toLocaleTimeString('zh-CN', {
                    hour: '2-digit', minute: '2-digit', hour12: false
                });

                batch.push({
                    text: msg.message,
                    type: isSelf ? 'self' : 'other',
                    senderName,
                    peerNumber,
                    avatarText,
                    timestamp,
                    ipAddress
                });
            });

            vsc.appendBatch(batch);
            vsc.scrollToBottom();
        })
        .catch(err => {
            console.error('Failed to load message history:', err);
        });
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
        .then(r => {
            // 读取 X-Server-Next-Id header，检测服务端重启
            const serverNextId = r.headers.get('X-Server-Next-Id');
            if (serverNextId) {
                const id = parseInt(serverNextId, 10);
                if (!isNaN(id) && id <= lastEventId) {
                    console.warn('[longPollEvents] Server restart detected, resetting lastEventId from', lastEventId, 'to 0');
                    lastEventId = 0;
                }
            }
            return r.json();
        })
        .then(events => {
            if (events && events.length > 0) {
                events.forEach(event => {
                    console.log('Event:', event);
                    lastEventId = event.event_id;
                    
                    if (event.event_type === 'friend_message') {
                        const data = JSON.parse(event.data);
                        if (data.friend_id == currentChatId && currentChatType === 'friend') {
                            // 对方消息：获取昵称首字母作为头像文本
                            var fe = peerInfoMap["friend_" + data.friend_id];
                            const senderName = (fe && fe.name) || '';
                            const avatarText = (senderName || String(data.friend_id)).charAt(0).toUpperCase();
                            appendMessage(data.message, 'other', senderName, data.friend_id, avatarText);
                        }
                    } else if (event.event_type === 'friend_name') {
                        const d = JSON.parse(event.data);
                        const k = "friend_" + d.friend_id;
                        if (peerInfoMap[k]) peerInfoMap[k].name = d.name;
                        const f = contacts.friends.find(f => f.friendId == d.friend_id);
                        if (f) f.name = d.name;
                        renderContactList(currentFilter);
                    } else if (event.event_type === 'friend_status') {
                        const d = JSON.parse(event.data);
                        const k = "friend_" + d.friend_id;
                        if (peerInfoMap[k]) {
                            peerInfoMap[k].status = d.status;
                            peerInfoMap[k].statusStr = d.status === 0 ? 'none' : d.status === 1 ? 'tcp' : 'udp';
                        }
                        const f = contacts.friends.find(f => f.friendId == d.friend_id);
                        if (f) f.status = d.status;
                        renderContactList(currentFilter);
                    } else if (event.event_type === 'self_connection_status') {
                        loadSelfInfo();
                    } else if (event.event_type === 'conference_invite') {
                        const data = JSON.parse(event.data);
                        showConferenceInviteDialog(data);
                    } else if (event.event_type === 'conference_message') {
                        const data = JSON.parse(event.data);
                        // 更新 peer info 缓存
                        if (data.peer_name) {
                            const key = `conference_${data.conference_number}_${data.peer_number}`;
                            peerInfoMap[key] = { ...(peerInfoMap[key] || {}), name: data.peer_name, peer_number: data.peer_number };
                        }
                        if (data.conference_number == currentChatId && currentChatType === 'conference') {
                            const key = `conference_${data.conference_number}_${data.peer_number}`;
                            const cached = peerInfoMap[key];
                            const senderName = (cached && cached.name) || '';
                            const avatarText = (senderName || 'P').charAt(0).toUpperCase();
                            appendMessage(data.message, 'other', senderName, data.peer_number, avatarText);
                        }
                    } else if (event.event_type === 'group_message') {
                        const data = JSON.parse(event.data);
                        // 更新 peer info 缓存
                        if (data.peer_name) {
                            const key = `group_${data.group_number}_${data.peer_number}`;
                            peerInfoMap[key] = { ...(peerInfoMap[key] || {}), name: data.peer_name, peer_number: data.peer_number };
                        }
                        if (data.group_number == currentChatId && currentChatType === 'group') {
                            const key = `group_${data.group_number}_${data.peer_number}`;
                            const cached = peerInfoMap[key];
                            const senderName = (cached && cached.name) || '';
                            const avatarText = (senderName || 'P').charAt(0).toUpperCase();
                            const peerIp = (cached && cached.peerIp) || '';
                            appendMessage(data.message, 'other', senderName, data.peer_number, avatarText, "", peerIp);
                        }
                    } else if (event.event_type === 'conference_peer_name') {
                        const data = JSON.parse(event.data);
                        const key = `conference_${data.conference_number}_${data.peer_number}`;
                        peerInfoMap[key] = { ...(peerInfoMap[key] || {}), name: data.name, peer_number: data.peer_number };
                    } else if (event.event_type === 'group_peer_name') {
                        const data = JSON.parse(event.data);
                        const key = `group_${data.group_number}_${data.peer_number}`;
                        peerInfoMap[key] = { ...(peerInfoMap[key] || {}), name: data.name, peer_number: data.peer_number };
                    } else if (event.event_type === 'group_invite') {
                        const data = JSON.parse(event.data);
                        currentEventId = event.id; // Save event ID for deletion
                        showGroupInviteDialog(data);
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

// 联系人信息缓存：键 "friend_{id}" / "group_{g}_{p}" / "conference_{c}_{p}"，值 { name, peerNumber, status, statusStr, ... }
let peerInfoMap = {};

// ── VirtualScroller ──
class VirtualScroller {
    constructor(container) {
        this.container = container;
        this.container.style.position = 'relative';
        this.buffer = 10;

        this.viewport = document.createElement('div');
        this.viewport.style.position = 'relative';
        this.viewport.style.width = '100%';
        this.container.appendChild(this.viewport);

        this.data = [];
        this.heights = [];
        this.cumulative = [0];
        this.totalHeight = 0;

        this.pool = [];
        this.usedSlots = new Map();
        this.freeSlots = [];

        this.startIndex = 0;
        this.endIndex = 0;

        this._rafId = null;
        this._measureRafId = null;
        this._measureQueue = new Set();

        this._onScroll = this._onScroll.bind(this);
        this._onResize = this._onResize.bind(this);
        this.container.addEventListener('scroll', this._onScroll, { passive: true });
        window.addEventListener('resize', this._onResize);

        requestAnimationFrame(() => this._ensurePool());
    }

    _ensurePool() {
        const viewH = this.container.clientHeight || 600;
        const visible = Math.ceil(viewH / 92);
        const target = visible + 2 * this.buffer + 5;
        while (this.pool.length < target) {
            const node = this._createNode();
            node.style.display = 'none';
            this.viewport.appendChild(node);
            this.freeSlots.push(this.pool.length);
            this.pool.push({ node, dataIndex: -1 });
        }
    }

    _createNode() {
        const el = document.createElement('div');
        el.className = 'message';
        el.style.position = 'absolute';
        el.style.left = '0';
        el.style.width = '100%';

        const avatarCol = document.createElement('div');
        avatarCol.className = 'avatar-col';
        avatarCol.innerHTML = '<div class="avatar-placeholder"></div>';

        const contentCol = document.createElement('div');
        contentCol.className = 'content-col';
        const header = document.createElement('div');
        header.className = 'message-header';
        header.innerHTML = '<span class="message-sender"></span><span class="message-ip"></span><span class="message-time"></span>';
        const bubble = document.createElement('div');
        bubble.className = 'message-bubble';
        contentCol.appendChild(header);
        contentCol.appendChild(bubble);

        // Translate button and text
        const transBtn = document.createElement('span');
        transBtn.className = 'translate-btn';
        transBtn.textContent = '\u{1F310}';
        bubble.appendChild(transBtn);

        const transText = document.createElement('div');
        transText.className = 'translated-text';
        contentCol.appendChild(transText);

        el.appendChild(avatarCol);
        el.appendChild(contentCol);

        el._ap = avatarCol.firstChild;
        el._ss = header.firstChild;
        el._ip = header.children[1];
        el._ts = header.lastChild;
        el._bb = bubble;
        el._tb = transBtn;
        el._tr = transText;
        el._ac = avatarCol;
        el._cc = contentCol;

        return el;
    }

    _updateNode(el, msg, top, dataIdx) {
        el.className = 'message ' + msg.type;
        el.style.top = top + 'px';
        el.style.display = '';

        el._ap.textContent = (msg.avatarText || '').toUpperCase();
        let displayText;
        if (msg.senderName)
            displayText = msg.senderName;
        else if (msg.peerNumber >= 0)
            displayText = 'Peer ' + msg.peerNumber;
        else
            displayText = '?';
        el._ss.textContent = displayText;
        if (msg.ipAddress && msg.type !== 'self') {
            el._ip.textContent = msg.ipAddress;
            el._ip.style.display = '';
        } else {
            el._ip.textContent = '';
            el._ip.style.display = 'none';
        }
        el._ts.textContent = msg.timestamp;

        // Render bubble content: text + translate button
        el._bb.innerHTML = linkifyText(msg.text);
        el._bb.className = 'message-bubble ' + (msg.type === 'self' ? 'self-bubble' : 'other-bubble');
        el._bb.appendChild(el._tb);

        // Translate button
        const isTranslated = msg.translatedText && msg.translatedText.length > 0;
        el._tb.style.display = 'inline';
        el._tb.title = msg.translateError || '翻译';
        el._tb.classList.toggle('error', !!msg.translateError);
        if (msg.translationInProgress) {
            el._tb.textContent = '\u23F3';
        } else {
            el._tb.textContent = '\u{1F310}';
        }
        el._tb._dataIdx = dataIdx;
        if (!el._tb._clickHandlerBound) {
            el._tb._clickHandlerBound = true;
            el._tb.addEventListener('click', (e) => {
                e.stopPropagation();
                translateMessage(el._tb._dataIdx);
            });
        }

        // Translated text
        if (msg.showTranslation && isTranslated) {
            el._tr.textContent = msg.translatedText;
            el._tr.style.display = 'block';
        } else {
            el._tr.textContent = '';
            el._tr.style.display = 'none';
        }

        if (msg.type === 'self') {
            if (el.dataset.layout !== 'self') {
                el.appendChild(el._cc);
                el.appendChild(el._ac);
                el.dataset.layout = 'self';
            }
        } else {
            if (el.dataset.layout !== 'other') {
                el.appendChild(el._ac);
                el.appendChild(el._cc);
                el.dataset.layout = 'other';
            }
        }

        this._requestMeasure(dataIdx);
    }

    _requestMeasure(dataIdx) {
        this._measureQueue.add(dataIdx);
        if (!this._measureRafId) {
            this._measureRafId = requestAnimationFrame(() => this._flushMeasure());
        }
    }

    _flushMeasure() {
        this._measureRafId = null;
        if (this._measureQueue.size === 0) return;
        let changed = false;
        let minIdx = Infinity;
        for (const dataIdx of this._measureQueue) {
            const pi = this.usedSlots.get(dataIdx);
            if (pi === undefined) continue;
            const actual = this.pool[pi].node.offsetHeight + 12;
            if (actual !== this.heights[dataIdx]) {
                this.heights[dataIdx] = actual;
                minIdx = Math.min(minIdx, dataIdx);
                changed = true;
            }
        }
        this._measureQueue.clear();
        if (changed && minIdx < this.data.length) {
            this._recalcCumulative(minIdx);
            this._render(true);
        }
    }

    _estimateHeight(text) {
        if (!text) return 92;
        const cw = this.container.clientWidth;
        if (!cw || cw < 100) return 92;
        const bubbleW = Math.max(50, (cw - 84) * 0.8);
        if (!this._ctx) {
            const canvas = document.createElement('canvas');
            this._ctx = canvas.getContext('2d');
            this._ctx.font = '14px -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif';
        }
        const textWidth = this._ctx.measureText(text).width;
        const lines = Math.max(1, Math.ceil(textWidth / bubbleW));
        return Math.max(92, 45 + lines * 21);
    }

    _recalcCumulative(from) {
        let acc = from > 0 ? this.cumulative[from] : 0;
        for (let i = from; i < this.data.length; i++) {
            this.cumulative[i] = acc;
            acc += this.heights[i];
        }
        this.cumulative[this.data.length] = acc;
        this.totalHeight = acc;
        this.viewport.style.height = this.totalHeight + 'px';
    }

    _calcStart(st) {
        const cum = this.cumulative;
        let lo = 0, hi = this.data.length;
        while (lo < hi) {
            const mid = (lo + hi) >>> 1;
            if (cum[mid] < st) lo = mid + 1; else hi = mid;
        }
        return Math.max(0, lo - this.buffer);
    }

    _calcEnd(st, ch) {
        const bottom = st + ch;
        const cum = this.cumulative;
        let lo = 0, hi = this.data.length;
        while (lo < hi) {
            const mid = (lo + hi) >>> 1;
            if (cum[mid] < bottom) lo = mid + 1; else hi = mid;
        }
        return Math.min(this.data.length, lo + this.buffer);
    }

    _render(forceUpdate) {
        this._ensurePool();
        const st = this.container.scrollTop;
        const ch = this.container.clientHeight;
        const ns = this._calcStart(st);
        const ne = this._calcEnd(st, ch);

        if (ns === this.startIndex && ne === this.endIndex && !forceUpdate) return;

        for (const [di, pi] of this.usedSlots) {
            if (di < ns || di >= ne) {
                this.pool[pi].node.style.display = 'none';
                this.pool[pi].dataIndex = -1;
                this.freeSlots.push(pi);
                this.usedSlots.delete(di);
            }
        }

        for (const [di, pi] of this.usedSlots) {
            if (di >= ns && di < ne) {
                const newTop = this.cumulative[di] + 'px';
                if (this.pool[pi].node.style.top !== newTop) {
                    this.pool[pi].node.style.top = newTop;
                }
            }
        }

        for (let i = ns; i < ne; i++) {
            if (this.usedSlots.has(i) || !this.data[i]) continue;
            let pi;
            if (this.freeSlots.length > 0) {
                pi = this.freeSlots.pop();
            } else {
                this._ensurePool();
                pi = this.freeSlots.pop();
                if (pi === undefined) break;
            }
            this.usedSlots.set(i, pi);
            this.pool[pi].dataIndex = i;
            this._updateNode(this.pool[pi].node, this.data[i], this.cumulative[i], i);
        }

        this.startIndex = ns;
        this.endIndex = ne;
    }

    _onScroll() {
        if (this._rafId) return;
        this._rafId = requestAnimationFrame(() => {
            this._rafId = null;
            this._render();
        });
    }

    _onResize() {
        if (this._resizeRafId) cancelAnimationFrame(this._resizeRafId);
        this._resizeRafId = requestAnimationFrame(() => {
            this._resizeRafId = null;
            for (let i = 0; i < this.data.length; i++) {
                this.heights[i] = this._estimateHeight(this.data[i].text);
            }
            this._recalcCumulative(0);
            this._render();
        });
    }

    append(msgData) {
        const idx = this.data.length;
        this.data.push(msgData);
        this.heights.push(this._estimateHeight(msgData.text));
        this._recalcCumulative(idx);
        this._render();
    }

    appendBatch(arr) {
        if (!arr || arr.length === 0) return;
        const from = this.data.length;
        for (const m of arr) {
            this.data.push(m);
            this.heights.push(this._estimateHeight(m.text));
        }
        this._recalcCumulative(from);
        this._render();
    }

    clear() {
        for (const [, pi] of this.usedSlots) {
            this.pool[pi].node.style.display = 'none';
            this.pool[pi].dataIndex = -1;
            this.freeSlots.push(pi);
        }
        this.usedSlots.clear();
        this.data = [];
        this.heights = [];
        this.cumulative = [0];
        this.totalHeight = 0;
        this.viewport.style.height = '0';
        this.startIndex = 0;
        this.endIndex = 0;
        this.container.scrollTop = 0;
    }

    scrollToBottom() {
        this.container.scrollTop = this.container.scrollHeight;
    }

    destroy() {
        this.container.removeEventListener('scroll', this._onScroll);
        window.removeEventListener('resize', this._onResize);
    }
}

const messageArea = document.getElementById('messageArea');
const vsc = new VirtualScroller(messageArea);

// Append message to chat area (VirtualScroller)
function appendMessage(text, type, senderName = "", peerNumber = -1, avatarText = "", timestamp = "", ipAddress = "") {
    const now = new Date();
    if (!timestamp)
        timestamp = now.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', hour12: false });
    vsc.append({ text, type, senderName, peerNumber, avatarText, timestamp, ipAddress });
    vsc.scrollToBottom();
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
    console.log('Group invite data:', data); // Debug log
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
        // Delete the event
        if (currentEventId > 0) {
            fetch(`/api/events?id=${currentEventId}`, { method: 'DELETE' });
        }
        const inviteData = data.chat_id || data.invite_data || data.cookie || '';
        if (!inviteData) {
            alert('Invalid invite: missing chat_id. Please check console for details.');
            console.error('Missing invite data:', data);
            return;
        }
        fetch('/api/groups/accept', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `invite_data=${encodeURIComponent(inviteData)}&friend_number=${data.friend_number}&name=${encodeURIComponent(data.name || '')}`
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
        // Delete the event
        if (currentEventId > 0) {
            fetch(`/api/events?id=${currentEventId}`, { method: 'DELETE' });
        }
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
        // Delete the event
        if (currentEventId > 0) {
            fetch(`/api/events?id=${currentEventId}`, { method: 'DELETE' });
        }
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
        body = `group_number=${currentChatId}&message=${encodeURIComponent(msg)}`;
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
            // 自己消息：传递 Me 和 M 作为头像文本
            appendMessage(msg, 'self', 'Me', -1, 'M');
            input.value = '';
            autoResizeTextarea();
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
                  updateChatHeader(null, null);
                  vsc.clear();
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
            } else if (action === 'rename') {
                window.showGroupRenameModal(selectedGroupId);
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
        body: `friend_ids=${friendId}`
    }).then(r => r.json())
      .then(data => {
          const f = data[0] || {};
          if (f.error) { alert(t('friend_info_failed') + ': ' + f.error); return; }
          document.getElementById('infoFriendName').textContent = f.name || t('no_name_label');
          document.getElementById('infoFriendId').textContent = f.friendId;
          document.getElementById('infoFriendStatus').textContent = f.statusText || t('unknown');
          document.getElementById('infoFriendConn').textContent = f.statusStr || t('unknown');
          document.getElementById('infoFriendPk').textContent = f.publicKey || t('unknown');
          
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
    // 从contacts.conferences中找到对应的会议数据
    const conf = contacts.conferences ? contacts.conferences.find(c => c.conferenceNumber == conferenceId) : null;
    document.getElementById('infoConferenceId').textContent = conferenceId;
    document.getElementById('infoConferenceType').textContent = 'Tox Conference';
    document.getElementById('infoConferenceConn').textContent = conf ? 
        (conf.isConnected ? '在线' : '离线') : 'N/A';
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
    // 从contacts.groups中找到对应的群组数据
    const group = contacts.groups ? contacts.groups.find(g => g.groupNumber == groupId) : null;
    document.getElementById('infoGroupId').textContent = groupId;
    document.getElementById('infoGroupName').textContent = group ? (group.groupName || 'N/A') : 'N/A';
    document.getElementById('infoGroupConn').textContent = group ? 
        (group.isConnected ? '在线' : '离线') : 'N/A';
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
        return data;
    } catch (err) {
        console.error('Failed to fetch group members:', err);
        return { members: [], selfPeerNumber: 0 };
    }
}

async function showConferenceMembers(conferenceId) {
    const members = await fetchConferenceMembers(conferenceId);
    const title = t('member_list.title.conference').replace('{0}', conferenceId);
    document.getElementById('memberListTitle').textContent = title;
    
    let html = '';
    if (members.length === 0) {
        html = '<tr><td colspan="6" style="text-align:center;color:#8b949e;">' + t('no_members') + '</td></tr>';
    } else {
        members.forEach(m => {
            html += `<tr>
                <td>${m.peer_number}</td>
                <td>${m.name || '?'}</td>
                <td>--</td>
                <td>--</td>
                <td>--</td>
                <td style="font-family:monospace;font-size:11px;">--</td>
            </tr>`;
        });
    }
    
    document.getElementById('memberListBody').innerHTML = html;
    document.getElementById('memberListModal').classList.remove('hidden');
}

async function showGroupMembers(groupId) {
    const result = await fetchGroupMembers(groupId);
    const members = result.members;
    const title = t('member_list.title.group').replace('{0}', groupId);
    document.getElementById('memberListTitle').textContent = title;
    
    let html = '';
    if (members.length === 0) {
        html = '<tr><td colspan="6" style="text-align:center;color:#8b949e;">' + t('no_members') + '</td></tr>';
    } else {
        members.forEach(m => {
            const roleStr = m.roleStr ? t('roles.' + m.roleStr) : '--';
            const connStr = m.statusStr || '--';
            const ipStr = m.peerIp || '--';
            const pkStr = (m.publicKey || '').substring(0, 16);
            html += `<tr>
                <td>${m.peerNumber}</td>
                <td>${m.name || '?'}</td>
                <td>${roleStr}</td>
                <td>${connStr}</td>
                <td>${ipStr}</td>
                <td style="font-family:monospace;font-size:11px;">${pkStr}</td>
            </tr>`;
        });
    }
    
    document.getElementById('memberListBody').innerHTML = html;
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
    
    // 合并为一次 POST 请求
    const params = new URLSearchParams();
    if (name) {
        params.append('name', name);
    }
    if (status) {
        params.append('status_message', status);
    }
    
    if (name || status) {
        fetch('/api/self', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: params.toString()
        })
        .then(() => {
            hideEditSelf();
            loadSelfInfo();
        })
        .catch(err => {
            alert(t('save_failed') + ': ' + err);
        });
    } else {
        hideEditSelf();
        loadSelfInfo();
    }
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
    initEmojiPicker();
    document.getElementById('messageInput').addEventListener('keydown', _messageInputHandler);
});

// ── Input helpers ──
function autoResizeTextarea() {
    var ta = document.getElementById('messageInput');
    if (!ta) return;
    ta.style.height = 'auto';
    ta.style.height = Math.min(ta.scrollHeight, 84) + 'px';
}

function selectFile() {
    alert(t('not_yet_implemented'));
}

// ── Emoji picker (built-in character array) ──
var EMOJI_GROUP_ICONS = {
    'Smileys & Emotion': '😀',
    'People & Body': '👋',
    'Symbols': '💎',
    'Activities': '⚽',
};
var EMOJI_GROUP_ORDER = ['Smileys & Emotion', 'People & Body', 'Symbols', 'Activities'];
var EMOJI_BY_GROUP = {
    'Smileys & Emotion': ['😀','😃','😄','😁','😅','😂','🤣','😊','😇','🙂','😉','😌','😍','🥰','😘','😗','😋','😛','😜','🤪','😝','🤑','🤗','🤭','🤫','🤔','🤐','😐','😑','😶','😏','😒','🙄','😬','🤥','😴','😷','🤒','🤕','🤢','🤮','🥴','😵','🤯','🤠','🥳','🥺','😢','😭','😤','😡','🤬'],
    'People & Body': ['💩','👍','👎','👊','✊','🤛','🤜','👏','🙌','👐','🤲','🤝','🙏','✌️','🤞'],
    'Symbols': ['❤️','🧡','💛','💚','💙','💜','🖤','🤍','🤎','💔','💕','💞','💗','💖','💘','✅','❌','⭕️','‼️','⁉️','❓','❔','❕','❗️','⚠️','🚫','🔞','📵','🚭','💢','♨️','💤','🌀','🔴','🟠','🟡','🟢','🔵','🟣','⚫️','⚪️','🟤','🔶','🔷','🔸','🔹','🔺','🔻','💬','🗯','💭'],
    'Activities': ['⭐️','🌟','✨','🔥','💯','🎉','🎊','🎈','🎁','🎀'],
};
var EMOJI_GROUPS = [];
for (var _egi = 0; _egi < EMOJI_GROUP_ORDER.length; _egi++) {
    if (EMOJI_BY_GROUP[EMOJI_GROUP_ORDER[_egi]]) EMOJI_GROUPS.push(EMOJI_GROUP_ORDER[_egi]);
}
var EMOJI_DATA = [];
for (var _edi = 0; _edi < EMOJI_GROUPS.length; _edi++) {
    var _g = EMOJI_GROUPS[_edi];
    for (var _ej = 0; _ej < EMOJI_BY_GROUP[_g].length; _ej++) {
        EMOJI_DATA.push(EMOJI_BY_GROUP[_g][_ej]);
    }
}
var _emojiPickerInit = false;
var _emojiActiveTab = EMOJI_GROUPS.length > 0 ? EMOJI_GROUPS[0] : null;

function getRecentEmoji() {
    try { return JSON.parse(localStorage.getItem('emoji_recent') || '[]'); }
    catch(e) { return []; }
}

function saveRecentEmoji(ch) {
    var recent = getRecentEmoji();
    recent = [ch].concat(recent.filter(function(c) { return c !== ch; }));
    if (recent.length > 20) recent = recent.slice(0, 20);
    localStorage.setItem('emoji_recent', JSON.stringify(recent));
}

function initEmojiPicker() {
    if (_emojiPickerInit) return;
    _emojiPickerInit = true;
    var root = document.getElementById('emojiPicker');
    if (!root) return;

    var searchDiv = document.createElement('div');
    searchDiv.className = 'emoji-search';
    var searchInput = document.createElement('input');
    searchInput.type = 'text';
    searchInput.placeholder = 'Search emoji...';
    searchInput.addEventListener('input', function() { renderEmojiGrid(root, searchInput.value); });
    searchDiv.appendChild(searchInput);
    root.appendChild(searchDiv);

    var tabsDiv = document.createElement('div');
    tabsDiv.className = 'emoji-tabs';
    for (var i = 0; i < EMOJI_GROUPS.length; i++) {
        var g = EMOJI_GROUPS[i];
        var tab = document.createElement('button');
        tab.className = 'emoji-tab' + (_emojiActiveTab === g ? ' active' : '');
        tab.textContent = EMOJI_GROUP_ICONS[g] || '?';
        tab.title = g;
        tab.addEventListener('click', (function(grp) { return function() { switchEmojiTab(root, grp, searchInput); }; })(g));
        tabsDiv.appendChild(tab);
    }
    root.appendChild(tabsDiv);

    var grid = document.createElement('div');
    grid.className = 'emoji-grid';
    root.appendChild(grid);

    renderEmojiGrid(root, '');
}

function switchEmojiTab(root, tab, searchInput) {
    _emojiActiveTab = tab;
    var tabButtons = root.querySelectorAll('.emoji-tab');
    for (var i = 0; i < tabButtons.length; i++) {
        tabButtons[i].className = 'emoji-tab';
    }
    var idx = EMOJI_GROUPS.indexOf(tab);
    if (idx >= 0 && tabButtons[idx]) tabButtons[idx].className = 'emoji-tab active';
    if (searchInput) searchInput.value = '';
    renderEmojiGrid(root, '');
}

function renderEmojiGrid(root, search) {
    var grid = root.querySelector('.emoji-grid');
    if (!grid) return;
    grid.innerHTML = '';

    if (search) {
        var lower = search.toLowerCase();
        for (var i = 0; i < EMOJI_DATA.length; i++) {
            if (EMOJI_DATA[i].toLowerCase().indexOf(lower) >= 0) {
                var btn = document.createElement('button');
                btn.className = 'emoji-item';
                btn.textContent = EMOJI_DATA[i];
                btn.addEventListener('click', (function(c) { return function() { insertEmoji(c); }; })(EMOJI_DATA[i]));
                grid.appendChild(btn);
            }
        }
        if (grid.children.length === 0) {
            grid.innerHTML = '<div class="emoji-label" style="text-align:center;padding:20px 0;">No results</div>';
        }
        return;
    }

    var hasContent = false;

    var recent = getRecentEmoji();
    if (recent.length > 0) {
        hasContent = true;
        var label = document.createElement('div');
        label.className = 'emoji-label';
        label.textContent = '🕓 Recently used';
        grid.appendChild(label);
        for (var i = 0; i < recent.length; i++) {
            var btn = document.createElement('button');
            btn.className = 'emoji-item';
            btn.textContent = recent[i];
            btn.addEventListener('click', (function(c) { return function() { insertEmoji(c); }; })(recent[i]));
            grid.appendChild(btn);
        }
    }

    var items = EMOJI_BY_GROUP[_emojiActiveTab] || [];
    if (items.length > 0) {
        hasContent = true;
        for (var i = 0; i < items.length; i++) {
            var btn = document.createElement('button');
            btn.className = 'emoji-item';
            btn.textContent = items[i];
            btn.addEventListener('click', (function(c) { return function() { insertEmoji(c); }; })(items[i]));
            grid.appendChild(btn);
        }
    }

    if (!hasContent) {
        grid.innerHTML = '<div class="emoji-label" style="text-align:center;padding:20px 0;">No emoji</div>';
    }
}

function insertEmoji(ch) {
    var input = document.getElementById('messageInput');
    if (!input) return;
    var start = input.selectionStart;
    var end = input.selectionEnd;
    var val = input.value;
    input.value = val.substring(0, start) + ch + val.substring(end);
    input.selectionStart = input.selectionEnd = start + ch.length;
    input.focus();
    autoResizeTextarea();
    saveRecentEmoji(ch);
    hideEmojiPicker();
}

function toggleEmojiPicker() {
    var root = document.getElementById('emojiPicker');
    if (!root) return;
    if (root.classList.contains('hidden')) {
        initEmojiPicker();
        root.classList.remove('hidden');
    } else {
        root.classList.add('hidden');
    }
}

function hideEmojiPicker() {
    var root = document.getElementById('emojiPicker');
    if (root) root.classList.add('hidden');
}

// Close emoji picker on outside click
document.addEventListener('click', function(e) {
    var picker = document.getElementById('emojiPicker');
    if (!picker || picker.classList.contains('hidden')) return;
    if (!e.target.closest('.input-area')) hideEmojiPicker();
});

// ── Switch account stub ──
function switchAccountStub() {
    alert(t('not_yet_implemented'));
}

// ── Group Rename ──
var renameGroupId = null;

function showGroupRenameModal(groupId) {
    renameGroupId = groupId;
    var modal = document.getElementById('groupRenameModal');
    if (!modal) return;
    modal.classList.remove('hidden');

    // Backdrop click to close
    modal.onclick = function(e) {
        if (e.target === modal) modal.classList.add('hidden');
    };

    document.querySelector('input[name="renameOpt"][value="self"]').checked = true;

    // Get global profile name
    var globalName = (typeof selfInfo !== 'undefined' && selfInfo && selfInfo.name) ? selfInfo.name : '';

    // Scan peerInfoMap for group-specific self nick
    var groupNick = '';
    var prefix = 'group_' + groupId + '_';
    for (var key in peerInfoMap) {
        if (key.indexOf(prefix) === 0 && peerInfoMap[key].isSelf) {
            groupNick = peerInfoMap[key].name;
            break;
        }
    }
    if (!groupNick) groupNick = globalName;

    // Get group name from DOM
    var item = document.querySelector('.list-item[data-group-id="' + groupId + '"]');
    var groupName = item ? item.querySelector('.item-text').textContent : ('群组 ' + groupId);
    document.getElementById('groupRenameTitle').textContent = t('rename.title') + ' - ' + groupName;

    document.getElementById('renameSelfPreview').textContent = globalName ? '(' + globalName + ')' : '(--)';
    document.getElementById('groupRenameCurrent').textContent = t('rename.current_nick') + ': ' + (groupNick || '--');
    document.getElementById('groupNickInput').value = groupNick;
    document.getElementById('groupNickInput').placeholder = t('rename.enter_nick');
    document.getElementById('groupNickInput').disabled = true;
    document.getElementById('renameRandomPreview').textContent = '--';
}

function closeGroupRenameModal() {
    var modal = document.getElementById('groupRenameModal');
    if (modal) modal.classList.add('hidden');
    renameGroupId = null;
}

function confirmGroupRename() {
    var opt = document.querySelector('input[name="renameOpt"]:checked');
    if (!opt) return;
    var name = '';
    if (opt.value === 'self') {
        name = (typeof selfInfo !== 'undefined' && selfInfo && selfInfo.name) ? selfInfo.name : '';
    } else if (opt.value === 'custom') {
        name = document.getElementById('groupNickInput').value.trim();
        if (!name) { alert(t('rename.name_empty')); return; }
    } else if (opt.value === 'random') {
        name = document.getElementById('renameRandomPreview').textContent;
        if (!name || name === '--') { alert(t('rename.generate_first')); return; }
    }
    doSetGroupName(renameGroupId, name);
}

function doSetGroupName(groupId, name) {
    var params = new URLSearchParams();
    params.append('group_number', groupId);
    params.append('name', name);
    fetch('/api/groups/set-name', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params.toString()
    }).then(function(r) { return r.json(); }).then(function(d) {
        if (d.error) { alert(t('rename.failed') + ': ' + d.error); return; }
        closeGroupRenameModal();
        loadAllData();
    });
}

function generateRandomName() {
    fetch('/api/random-name').then(function(r) { return r.json(); }).then(function(d) {
        var preview = document.getElementById('renameRandomPreview');
        if (preview) preview.textContent = d.name;
    });
}

// Radio click handler for rename options
document.addEventListener('click', function(e) {
    if (e.target.matches('input[name="renameOpt"]')) {
        var val = e.target.value;
        var inp = document.getElementById('groupNickInput');
        inp.disabled = (val !== 'custom');
        if (val === 'custom') inp.focus();
        if (val === 'random') generateRandomName();
    }
});

// Enter key sends message (Shift+Enter for newline)
var _messageInputHandler = function(e) {
    if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        sendMessage();
    }
};

// ESC key closes modals and emoji picker
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        hideFriendInfo();
        hideAllContextMenus();
        hideEditSelf();
        hideConferenceInfo();
        hideGroupInfo();
        hideSelectConference();
        hideMemberList();
        closeGroupRenameModal();
        hideEmojiPicker();
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
window.toggleEmojiPicker = toggleEmojiPicker;
window.selectFile = selectFile;
window.switchAccountStub = switchAccountStub;
window.showGroupRenameModal = showGroupRenameModal;
window.closeGroupRenameModal = closeGroupRenameModal;
window.confirmGroupRename = confirmGroupRename;
