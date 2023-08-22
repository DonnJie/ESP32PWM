#ifndef _FAST_PROV_SERVER_H_
#define _FAST_PROV_SERVER_H_

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#include "board.h"
#include "ble_mesh_fast_prov_operation.h"
#include "ble_mesh_fast_prov_client_model.h"
#include "ble_mesh_fast_prov_server_model.h"
#include "ble_mesh_example_init.h"

#define TAG "EXAMPLE"


// 定时器,发送自配网的节点地址
extern struct k_delayed_work send_self_prov_node_addr_timer;
// Fast Prov Client 模型的标志
extern bt_mesh_atomic_t fast_prov_cli_flags;

// 配网开始的节点数量
static uint8_t prov_start_num = 2;
//配网判断码
static bool prov_start = false;

// 配置客户端模型实例
esp_ble_mesh_client_t config_client;

// Fast Prov 客户端模型实例
esp_ble_mesh_client_t fast_prov_client = {
    .op_pair_size = ARRAY_SIZE(fast_prov_cli_op_pair),
    .op_pair = fast_prov_cli_op_pair,
};


example_fast_prov_server_t fast_prov_server = {
     // 该设备是主要的 Provisioner
    .primary_role  = false,
    // 最大可配网节点数,配网节点数不能超过该数字
    .max_node_num  = 6,
    // 已配网节点数
    .prov_node_cnt = 0x0,
    // 可分配的最小和最大单播地址
    .unicast_min   = ESP_BLE_MESH_ADDR_UNASSIGNED,
    .unicast_max   = ESP_BLE_MESH_ADDR_UNASSIGNED,
    // 当前要分配的单播地址  
    .unicast_cur   = ESP_BLE_MESH_ADDR_UNASSIGNED,
     // 每次分配的单播地址步长
    .unicast_step  = 0x0,
     // 一些标志位
    .flags         = 0x0,
    // 当前网络的 IV Index
    .iv_index      = 0x0,
    // 当前使用的 NetKey 索引
    .net_idx       = ESP_BLE_MESH_KEY_UNUSED,
     // 当前使用的 AppKey 索引
    .app_idx       = ESP_BLE_MESH_KEY_UNUSED,
    // 要分配的组地址
    .group_addr    = ESP_BLE_MESH_ADDR_UNASSIGNED,
    // 主 Provisioner 的单播地址
    .prim_prov_addr = ESP_BLE_MESH_ADDR_UNASSIGNED,
    // 设备匹配值相关
    .match_len     = 0x0,
    // 当前 pending 操作
    .pend_act      = FAST_PROV_ACT_NONE,
    // 当前状态
    .state         = STATE_IDLE,
};

// 配置服务器模型实例
esp_ble_mesh_cfg_srv_t config_server = {
    // 中继模式配置
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    // 信标广播配置
    .beacon = ESP_BLE_MESH_BEACON_DISABLED,
    // 好友功能配置
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif

  // GATT代理配置  
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
  // 默认消息TTL值  
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    // 网络层传输计数器和间隔步骤
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    // 中继重传计数器和间隔步骤
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};



static uint8_t dev_uuid[16] = { 0xdd, 0xdd };
void fast_prov_server_init(void);


#endif