#pragma once

/*
 * server和client的公共文件
 */
enum EnMsgTpye
{
    LOGIN_MSG = 1,  // 登录消息
    LOGIN_MSG_ACK,  // 登录响应消息
    REG_MSG,        // 注册消息
    REG_MSG_ACK,    // 注册响应消息
    ONE_CHAT_MSG,   // 聊天消息
    ADD_FRIEND_MSG, // 添加好友
    DEL_FRIEND_MSG, // 删除好友

    CREATE_GROUP_MSG, // 创建群组
    ADD_GROUP_MSG,    // 加入群组
    QUIT_GROUP_MSG,   // 退出群组
    GROUP_CHAT_MSG,   // 群聊消息
};