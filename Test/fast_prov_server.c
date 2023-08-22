#include "fast_prov_server.h"

//节点初始化函数
static void node_prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx: 0x%04x, unicast_addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08" PRIx32, flags, iv_index);
    board_prov_complete(); //关闭绿灯
    /* Updates the net_idx used by Fast Prov Server model, and it can also
     * be updated if the Fast Prov Info Set message contains a valid one.
     */
    fast_prov_server.net_idx = net_idx; // 更新fast_prov_server模型内部的net_idx值 ,还不清楚有什么用
}

// 当接收到未配网设备的广播数据包时,会调用这个函数启动配网流程
static void example_recv_unprov_adv_pkt(uint8_t dev_uuid[16], uint8_t addr[BLE_MESH_ADDR_LEN],
                                        esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
                                        uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{
     // 定义参数结构体,用于后面添加未配网设备 
    esp_ble_mesh_unprov_dev_add_t add_dev = {0};
    // 定义添加设备的标志位
    esp_ble_mesh_dev_add_flag_t flag;
    esp_err_t err;

    /* In Fast Provisioning, the Provisioner should only use PB-ADV to provision devices. */
    /* 如果是快速配网模式,配网仪只使用广播通道配网 */
    if (prov_start && (bearer & ESP_BLE_MESH_PROV_ADV)) {
        /* 检查设备是否已经配网过,避免重复配网 */
        /* Checks if the device is a reprovisioned one. */
        if (example_is_node_exist(dev_uuid) == false) {
            // 检查未配网设备数和已配网设备数是否超过限制
            if ((prov_start_num >= fast_prov_server.max_node_num) ||
                    (fast_prov_server.prov_node_cnt >= fast_prov_server.max_node_num)) {
                        // 超过限制则返回
                return;
            }
        }

        // 准备参数
        add_dev.addr_type = (uint8_t)addr_type;  //是设备的地址类型,比如公共地址或随机地址,需要转换为uint8_t类型
        add_dev.oob_info = oob_info;  // 是外带的配网信息,用于认证。
        add_dev.bearer = (uint8_t)bearer; // 是承载方式,包括广播、GATT等。
        memcpy(add_dev.uuid, dev_uuid, 16); //是设备的UUID,用于唯一标识设备,需要复制到参数结构体中。
        memcpy(add_dev.addr, addr, BLE_MESH_ADDR_LEN); //设备的MAC地址,需要复制到参数结构体中。
        // 设置添加设备的标志位
        flag = ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG;
        // 调用API添加未配网设备,启动配网，配网API
        err = esp_ble_mesh_provisioner_add_unprov_dev(&add_dev, flag);
        if (err != ESP_OK) {
            // 配网失败的处理
            ESP_LOGE(TAG, "%s: Failed to start provisioning device", __func__);
            return;
        }

        /* If adding unprovisioned device successfully, increase prov_start_num */
       // 配网成功,增加计数
        prov_start_num++;
    }

    return;
}

// 当配网链路打开时被调用的回调函数  
static void provisioner_prov_link_open(esp_ble_mesh_prov_bearer_t bearer)
{
    // 打印日志,输出正在使用的配网承载方式(PB-ADV或PB-GATT)
    ESP_LOGI(TAG, "%s: bearer %s", __func__, bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
}

// 当配网链路关闭时被调用的回调函数
static void provisioner_prov_link_close(esp_ble_mesh_prov_bearer_t bearer, uint8_t reason)
{
    // 打印关闭原因和配网承载方式
    ESP_LOGI(TAG, "%s: bearer %s, reason 0x%02x", __func__,
             bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT", reason);
    // 如果有正在配网的设备数,则减一
    if (prov_start_num) {
        prov_start_num--;
    }
}


static void provisioner_prov_complete(int node_idx, const uint8_t uuid[16], uint16_t unicast_addr,
                                      uint8_t element_num, uint16_t net_idx)
{
    // 节点信息结构体
    example_node_info_t *node = NULL;
    esp_err_t err;

    // 检查是否是新节点,如果是则新增节点计数
    if (example_is_node_exist(uuid) == false) {
        fast_prov_server.prov_node_cnt++;
    }

    // 打印设备UUID
    ESP_LOG_BUFFER_HEX("Device uuid", uuid + 2, 6);
    // 打印分配的单播地址
    ESP_LOGI(TAG, "Unicast address 0x%04x", unicast_addr);

    /* Sets node info */
    // 保存节点信息, API?
    err = example_store_node_info(uuid, unicast_addr, element_num, net_idx,
                                  fast_prov_server.app_idx, LED_OFF);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Failed to set node info", __func__);
        return;
    }

    /* Gets node info */
    // 获取节点信息 ,API? 
    node = example_get_node_info(unicast_addr);
    if (!node) {
        ESP_LOGE(TAG, "%s: Failed to get node info", __func__);
        return;
    }

    // 主配网仪特有的处理
    if (fast_prov_server.primary_role == true) {
        /* If the Provisioner is the primary one (i.e. provisioned by the phone), it shall
         * store self-provisioned node addresses;
         * If the node_addr_cnt configured by the phone is small than or equal to the
         * maximum number of nodes it can provision, it shall reset the timer which is used
         * to send all node addresses to the phone.
         */
        // 存储自己配网的节点地址
        err = example_store_remote_node_address(unicast_addr);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "%s: Failed to store node address 0x%04x", __func__, unicast_addr);
            return;
        }
        // 节点数量小于最大数,重置启用GATT代理的计时器
        if (fast_prov_server.node_addr_cnt != FAST_PROV_NODE_COUNT_MIN &&
            fast_prov_server.node_addr_cnt <= fast_prov_server.max_node_num) {
#pragma GCC diagnostic push
#if     __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif
            if (bt_mesh_atomic_test_and_clear_bit(fast_prov_server.srv_flags, GATT_PROXY_ENABLE_START)) {
                k_delayed_work_cancel(&fast_prov_server.gatt_proxy_enable_timer);
            }
            if (!bt_mesh_atomic_test_and_set_bit(fast_prov_server.srv_flags, GATT_PROXY_ENABLE_START)) {
                k_delayed_work_submit(&fast_prov_server.gatt_proxy_enable_timer, GATT_PROXY_ENABLE_TIMEOUT);
            }
#pragma GCC diagnostic pop
        }
    } else {
        /* When a device is provisioned, the non-primary Provisioner shall reset the timer
         * which is used to send node addresses to the primary Provisioner.
         */
        if (bt_mesh_atomic_test_and_clear_bit(&fast_prov_cli_flags, SEND_SELF_PROV_NODE_ADDR_START)) {
            k_delayed_work_cancel(&send_self_prov_node_addr_timer);
        }
        // 重置发送自己配网节点地址的计时器
        if (!bt_mesh_atomic_test_and_set_bit(&fast_prov_cli_flags, SEND_SELF_PROV_NODE_ADDR_START)) {
            k_delayed_work_submit(&send_self_prov_node_addr_timer, SEND_SELF_PROV_NODE_ADDR_TIMEOUT);
        }
    }

#pragma GCC diagnostic push
#if     __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif
    // 检查是否需要停止配网
    if (bt_mesh_atomic_test_bit(fast_prov_server.srv_flags, DISABLE_FAST_PROV_START)) {
        /* When a device is provisioned, and the stop_prov flag of the Provisioner has been
         * set, the Provisioner shall reset the timer which is used to stop the provisioner
         * functionality.
         */
        // 重置停止配网的计时器
        k_delayed_work_cancel(&fast_prov_server.disable_fast_prov_timer);
        k_delayed_work_submit(&fast_prov_server.disable_fast_prov_timer, DISABLE_FAST_PROV_TIMEOUT);
    }
#pragma GCC diagnostic pop

    /* The Provisioner will send Config AppKey Add to the node. */
     // 发送配置应用密钥添加消息
    example_msg_common_info_t info = {
        .net_idx = node->net_idx,
        .app_idx = node->app_idx,
        .dst = node->unicast_addr,
        .timeout = 0,
        .role = ROLE_FAST_PROV,
    };
    err = example_send_config_appkey_add(config_client.model, &info, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Failed to send Config AppKey Add message", __func__);
        return;
    }
}


static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
        esp_ble_mesh_prov_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    // 配网模块注册完成事件
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code: %d",
                 param->prov_register_comp.err_code);
        break;
      // 使能配网功能完成事件 
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code: %d",
                 param->node_prov_enable_comp.err_code);
        break;
        // 配网链接打开事件  
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer: %s",
                 param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
        // 配网链接关闭事件  
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer: %s",
                 param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
        // 节点配网完成事件  
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT"); 
        //节点初始化函数
        node_prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr, //这个函数包括关闭绿灯的操作
            param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
        // GATT代理关闭完成事件
    case ESP_BLE_MESH_NODE_PROXY_GATT_DISABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROXY_GATT_DISABLE_COMP_EVT");
         // 如果是主要的Provisioner角色
        if (fast_prov_server.primary_role == true) {
            // 禁用中继功能  
            config_server.relay = ESP_BLE_MESH_RELAY_DISABLED;
        }
         // 开始配网,配网的一个判断码
        prov_start = true;
        break;
        // 接收到未配网设备广播包 ，接收到未配网设备才会进行此操作 
    case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT:
        //设备会调用provisioner_add_unprov_dev()启动配网。
        example_recv_unprov_adv_pkt(param->provisioner_recv_unprov_adv_pkt.dev_uuid, param->provisioner_recv_unprov_adv_pkt.addr,
                                    param->provisioner_recv_unprov_adv_pkt.addr_type, param->provisioner_recv_unprov_adv_pkt.oob_info,
                                    param->provisioner_recv_unprov_adv_pkt.adv_type, param->provisioner_recv_unprov_adv_pkt.bearer);
        break;
        // 配网链接打开事件
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT");
        // 配网链接建立时调用。
        provisioner_prov_link_open(param->provisioner_prov_link_open.bearer);
        break;
        // 配网链接关闭事件
    case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT");
         //配网链接关闭时调用。
        provisioner_prov_link_close(param->provisioner_prov_link_close.bearer,
                                    param->provisioner_prov_link_close.reason);
        break;
        // Provisioner配网完成事件
    case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT");
        //配网完全结束时调用，该函数负责初始化新的节点,包括记录信息、分配地址、发送消息等，这个函数非常重要,执行新节点加入网上的必要步骤。
        provisioner_prov_complete(param->provisioner_prov_complete.node_idx,
                                  param->provisioner_prov_complete.device_uuid,
                                  param->provisioner_prov_complete.unicast_addr,
                                  param->provisioner_prov_complete.element_num,
                                  param->provisioner_prov_complete.netkey_idx);
        break;
        // 添加未配网设备完成事件 ,在  example_recv_unprov_adv_pkt() 之后才会执行
    case ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT, err_code: %d",
                 param->provisioner_add_unprov_dev_comp.err_code);
        break;
        // 设置UUID匹配完成事件 
    case ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT, err_code: %d",
                 param->provisioner_set_dev_uuid_match_comp.err_code);
        break;
    // 设置节点名称完成事件
    case ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT, err_code: %d",
                 param->provisioner_set_node_name_comp.err_code);
        break;
        // 设置快速配网信息完成事件
    case ESP_BLE_MESH_SET_FAST_PROV_INFO_COMP_EVT: {
        ESP_LOGI(TAG, "ESP_BLE_MESH_SET_FAST_PROV_INFO_COMP_EVT");
        ESP_LOGI(TAG, "status_unicast: 0x%02x, status_net_idx: 0x%02x, status_match 0x%02x",
                 param->set_fast_prov_info_comp.status_unicast,
                 param->set_fast_prov_info_comp.status_net_idx,
                 param->set_fast_prov_info_comp.status_match);
                 // 调用处理函数
        //该函数在快速配网信息设置完成时调用。 它会根据status代码判断设置是否成功。 如果成功,将更新节点的unicast地址范围、net_idx等信息。
        err = example_handle_fast_prov_info_set_comp_evt(fast_prov_server.model,
                param->set_fast_prov_info_comp.status_unicast,
                param->set_fast_prov_info_comp.status_net_idx,
                param->set_fast_prov_info_comp.status_match);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "%s: Failed to handle Fast Prov Info Set complete event", __func__);
            return;
        }
        break;
    }
    case ESP_BLE_MESH_SET_FAST_PROV_ACTION_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_SET_FAST_PROV_ACTION_COMP_EVT, status_action 0x%02x",
                 param->set_fast_prov_action_comp.status_action);
        //该函数在快速配网动作设置完成时调用。 它会根据status代码判断设置是否成功。
        err = example_handle_fast_prov_action_set_comp_evt(fast_prov_server.model,
                param->set_fast_prov_action_comp.status_action);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "%s: Failed to handle Fast Prov Action Set complete event", __func__);
            return;
        }
        break;
    default:
        break;
    }

    return;
}

/*
********************************************************************************************************
example_ble_mesh_custom_model_cb回调函数的主要作用如下:

    将BLE Mesh协议栈接收到的与快速配网相关的模型消息,上报给应用层处理。
    对不同的快速配网模型操作码进行区分处理。
    当发送快速配网状态消息完成时,触发后续的配网流程控制。
    当客户端模型消息发送超时时,调用重传或其他超时处理。
    作为自定义模型的回调函数,接收mesh协议栈的关键事件,实现应用层的处理。

总结起来,这个回调函数的作用是:

    将协议栈模型事件桥接到应用层,用于实现用户定制的快速配网业务逻辑。
    处理配网关键消息和状态变化,推进配网流程。
    实现定制化的模型消息重传或者超时处理。

它是连接BLE Mesh协议栈和应用层的纽带,使应用层可以接收配网相关事件并进行处理,是实现快速配网服务的关键。
*********************************************************************************************************
*/
// 自定义模型的回调函数
static void example_ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
        esp_ble_mesh_model_cb_param_t *param)
{
    uint32_t opcode;
    esp_err_t err;

    // 根据事件类型处理
    switch (event) {
    // 模型操作事件 
    case ESP_BLE_MESH_MODEL_OPERATION_EVT: {
        if (!param->model_operation.model || !param->model_operation.model->op ||
                !param->model_operation.ctx) {
            ESP_LOGE(TAG, "%s: model_operation parameter is NULL", __func__);
            return;
        }
        // 获取操作码
        opcode = param->model_operation.opcode;
        switch (opcode) {
        // 以下是快速配网服务器模型的操作码处理
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_INFO_SET:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NET_KEY_ADD:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_ADDR:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_ADDR_GET:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_GROUP_ADD:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_GROUP_DELETE: {
            // 处理快速配网服务器模型的消息 ,打印日志,表示收到该删除节点组消息
            ESP_LOGI(TAG, "%s: Fast prov server receives msg, opcode 0x%04" PRIx32, __func__, opcode);
            // 将mesh协议栈上报的消息参数,包装到net_buf_simple结构体中
            struct net_buf_simple buf = {
                .len = param->model_operation.length,
                .data = param->model_operation.msg,
            };
           // 调用示例函数example_fast_prov_server_recv_msg去处理消息。 删除？
            err = example_fast_prov_server_recv_msg(param->model_operation.model,
                                                    param->model_operation.ctx, &buf);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s: Failed to handle fast prov client message", __func__);
                return;
            }
            break;
        }
        // 以下是快速配网客户端模型的操作码处理   
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_INFO_STATUS:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NET_KEY_STATUS:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_ADDR_ACK: {
            ESP_LOGI(TAG, "%s: Fast prov client receives msg, opcode 0x%04" PRIx32, __func__, opcode);
            err = example_fast_prov_client_recv_status(param->model_operation.model,
                    param->model_operation.ctx,
                    param->model_operation.length,
                    param->model_operation.msg);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s: Failed to handle fast prov server message", __func__);
                return;
            }
            break;
        }
        // 其他未处理的操作码
        default:
            ESP_LOGI(TAG, "%s: opcode 0x%04" PRIx32, __func__, param->model_operation.opcode);
            break;
        }
        break;
    }
    // 发送完成事件
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_SEND_COMP_EVT, err_code %d", param->model_send_comp.err_code);
        //根据发送消息的opcode进行区分处理。
        switch (param->model_send_comp.opcode) {
        // 处理快速配网状态消息的发送完成事件
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_INFO_STATUS:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NET_KEY_STATUS:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_ADDR_ACK:
        case ESP_BLE_MESH_VND_MODEL_OP_FAST_PROV_NODE_ADDR_STATUS:
        //它的主要作用是处理快速配网状态消息发送完成后的事件
            err = example_handle_fast_prov_status_send_comp_evt(param->model_send_comp.err_code,
                    param->model_send_comp.opcode,
                    param->model_send_comp.model,
                    param->model_send_comp.ctx);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s: Failed to handle fast prov status send complete event", __func__);
                return;
            }
            break;
        default:
            break;
        }
        break;
    case ESP_BLE_MESH_MODEL_PUBLISH_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_PUBLISH_COMP_EVT, err_code %d",
                 param->model_publish_comp.err_code);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_CLIENT_RECV_PUBLISH_MSG_EVT, opcode 0x%04" PRIx32,
                 param->client_recv_publish_msg.opcode);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT, opcode 0x%04" PRIx32 ", dst 0x%04x",
                 param->client_send_timeout.opcode, param->client_send_timeout.ctx->addr);
        //客户端模型消息发送超时后,实现重传或相关处理
        err = example_fast_prov_client_recv_timeout(param->client_send_timeout.opcode,
                param->client_send_timeout.model,
                param->client_send_timeout.ctx);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "%s: Faield to resend fast prov client message", __func__);
            return;
        }
        break;
    default:
        break;
    }
}

/*****************************************************************
 
 主要作用是:

    处理配置客户端模型的相关事件。
    当接收到添加应用密钥的确认时,触发发起快速配网过程。
    检查节点地址分配是否足够,如果不够只设置组地址。
    构造并下发快速配网参数消息,通知节点进入快速配网。
    定时重发添加应用密钥等消息。

简言之,这个回调函数通过监听配置客户端模型的事件,以添加应用密钥的确认作为发起快速配网的触发条件。

接收到添加应用密钥确认事件后,它负责检查地址资源,构造快速配网参数消息,下发给节点设备,通知其进入快速配网过程。

同时也实现了配置消息的重传处理。

 * *****************************************************************
*/
// 配置客户端模型的回调函数 
static void example_ble_mesh_config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
        esp_ble_mesh_cfg_client_cb_param_t *param)
{
    // 节点信息结构体
    example_node_info_t *node = NULL;
    // 操作码
    uint32_t opcode;
    // 设备地址
    uint16_t address;
    // 返回值
    esp_err_t err;

    // 打印日志
    ESP_LOGI(TAG, "%s, error_code = 0x%02x, event = 0x%02x, addr: 0x%04x",
             __func__, param->error_code, event, param->params->ctx.addr);

    // 获取地址和操作码
    opcode = param->params->opcode;
    address = param->params->ctx.addr;

    // 根据地址获取节点信息
    node = example_get_node_info(address);
    if (!node) {
        ESP_LOGE(TAG, "%s: Failed to get node info", __func__);
        return;
    }

    // 检查参数错误码
    if (param->error_code) {
        ESP_LOGE(TAG, "Failed to send config client message, opcode: 0x%04" PRIx32, opcode);
        return;
    }

    // 根据事件类型处理
    switch (event) {
    case ESP_BLE_MESH_CFG_CLIENT_GET_STATE_EVT:
        break;
    case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
        switch (opcode) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD: {
            example_fast_prov_info_set_t set = {0};
            if (node->reprov == false) {
                /* After sending Config AppKey Add successfully, start to send Fast Prov Info Set */
                if (fast_prov_server.unicast_cur >= fast_prov_server.unicast_max) {
                    /* TODO:
                     * 1. If unicast_cur is >= unicast_max, we can also send the message to enable
                     * the Provisioner functionality on the node, and need to add another vendor
                     * message used by the node to require a new unicast address range from primary
                     * Provisioner, and before get the correct response, the node should pend
                     * the fast provisioning functionality.
                     * 2. Currently if address is not enough, the Provisioner will only add the group
                     * address to the node.
                     */
                    ESP_LOGW(TAG, "%s: Not enough address to be assigned", __func__);
                    node->lack_of_addr = true;
                } else {
                    /* Send fast_prov_info_set message to node */
                    // 发送快速配网参数
                    node->lack_of_addr = false;
                    node->unicast_min = fast_prov_server.unicast_cur;
                    if (fast_prov_server.unicast_cur + fast_prov_server.unicast_step >= fast_prov_server.unicast_max) {
                        node->unicast_max = fast_prov_server.unicast_max;
                    } else {
                        node->unicast_max = fast_prov_server.unicast_cur + fast_prov_server.unicast_step;
                    }
                    node->flags      = fast_prov_server.flags;
                    node->iv_index   = fast_prov_server.iv_index;
                    node->fp_net_idx = fast_prov_server.net_idx;
                    node->group_addr = fast_prov_server.group_addr;
                    node->prov_addr  = fast_prov_server.prim_prov_addr;
                    node->match_len  = fast_prov_server.match_len;
                    memcpy(node->match_val, fast_prov_server.match_val, fast_prov_server.match_len);
                    node->action = FAST_PROV_ACT_ENTER;
                    fast_prov_server.unicast_cur = node->unicast_max + 1;
                }
            }
            if (node->lack_of_addr == false) {
                set.ctx_flags = 0x03FE;
                memcpy(&set.unicast_min, &node->unicast_min,
                       sizeof(example_node_info_t) - offsetof(example_node_info_t, unicast_min));
            } else {
                set.ctx_flags  = BIT(6);
                set.group_addr = fast_prov_server.group_addr;
            }
            example_msg_common_info_t info = {
                .net_idx = node->net_idx,
                .app_idx = node->app_idx,
                .dst = node->unicast_addr,
                .timeout = 0,
                .role = ROLE_FAST_PROV,
            };
            err = example_send_fast_prov_info_set(fast_prov_client.model, &info, &set);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s: Failed to send Fast Prov Info Set message", __func__);
                return;
            }
            break;
        }
        default:
            break;
        }
        break;
    case ESP_BLE_MESH_CFG_CLIENT_PUBLISH_EVT:
        break;
    case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
        switch (opcode) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD: {
            example_msg_common_info_t info = {
                .net_idx = node->net_idx,
                .app_idx = node->app_idx,
                .dst = node->unicast_addr,
                .timeout = 0,
                .role = ROLE_FAST_PROV,
            };
            err = example_send_config_appkey_add(config_client.model, &info, NULL);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "%s: Failed to send Config AppKey Add message", __func__);
                return;
            }
            break;
        }
        default:
            break;
        }
        break;
    default:
        return;
    }
}


static esp_err_t ble_mesh_init(void) 
{
    esp_err_t err;
    //注册配网过程的回调函数，回调函数会处理配网中的各种事件，如节点配网成功、失败以及为收到配网相关消息等
    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    //注册自定义模型回调函数，会处理自定义模型的消息
    esp_ble_mesh_register_custom_model_callback(example_ble_mesh_custom_model_cb);
    //注册配置客户端模型的回调函数，处理配置客户端消息的发送结果
    esp_ble_mesh_register_config_client_callback(example_ble_mesh_config_client_cb);



    // 初始化 BLE Mesh 协议栈
    //err = esp_ble_mesh_init(&prov, &comp);

    return err;
}

void fast_prov_server_init(void)
{

        ble_mesh_get_dev_uuid(dev_uuid);
        ble_mesh_init();

    
}