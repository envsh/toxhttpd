新 Group Chat API 扩展已完成！编译成功。
完成的工作
修改的文件
1. const.go - 添加 Group Chat 类型和常量：
   - GroupNumber, GroupPeerNumber, GroupMessageId 类型
   - GroupPrivacyState, GroupTopicLock, GroupVoiceState, GroupRole, GroupExitType, GroupJoinFail, GroupModEvent 枚举类型
   - 对应的常量定义
2. const_auto.go - 添加 Group Chat 错误码：
   - 20+ 个错误枚举的常量和错误消息
   - 修正了不存在的 C 常量引用
3. tox.go - 扩展 Tox 结构体：
   - 添加 17 个新的回调函数 map（cb_group_*）
4. groupchat.go - 新文件，实现新 Group Chat API：
   - 最大值查询（8个）：GroupMaxTopicLength(), GroupMaxPartLength() 等
   - 枚举转字符串（6个）：GroupPrivacyStateToString(), GroupRoleToString() 等
   - Group 管理（5个）：GroupNew(), GroupJoin(), GroupLeave() 等
   - Self 信息（7个）：GroupSelfSetName(), GroupSelfGetRole() 等
   - Peer 查询（5个）：GroupPeerGetName(), GroupPeerGetRole() 等
   - Group 状态（15+个）：GroupGetName(), GroupSetTopic(), GroupSetPrivacyState() 等
   - 消息发送（4个）：GroupSendMessage(), GroupSendCustomPacket() 等
   - 邀请/管理（5个）：GroupInviteFriend(), GroupSetRole(), GroupKickPeer() 等
   - 回调注册（15+个）：CallbackGroupPeerName(), CallbackGroupMessage(), CallbackGroupModeration() 等
关键特性
- ✅ 去掉了 tox_ 前缀（Go binding 风格）
- ✅ 去掉了 Tox_Group_ 前缀（类型名简化）
- ✅ 去掉了 TOX_GROUP_ 前缀（常量简化）
- ✅ 使用 cb_group_ 前缀（与 cb_conference_ 一致）
- ✅ 遵循现有代码模式（CGo 包装器、putcbevts、错误处理）
下一步
现在可以使用新的 Group Chat API 了。需要我继续实现：
1. 后端 main.go 调用新 API？
2. 前端 Qt 界面支持新 Group Chat？
3. 还是其他任务？
