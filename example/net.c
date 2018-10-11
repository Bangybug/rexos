/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2017, Alexey Kramarenko
    All rights reserved.
*/

#include "net.h"
#include "app_private.h"
#include "config.h"
#include "usb_desc.h"
#include "../../rexos/userspace/eth.h"
#include "../../rexos/userspace/ip.h"
#include "../../rexos/userspace/icmp.h"
#include "../../rexos/userspace/tcp.h"
#include "../../rexos/userspace/tcpip.h"
#include "../../rexos/userspace/stm32/stm32_driver.h"
#include "../../rexos/userspace/stdio.h"
#include "../../rexos/userspace/pin.h"
#include "../../rexos/userspace/rndis.h"
#include "../../rexos/userspace/web.h"
#include <string.h>

static const MAC __MAC =                    {{0x20, 0xD9, 0x97, 0xA1, 0x90, 0x42}};
static const IP __IP =                      {{192, 168, 8, 126}};

void net_init(APP* app)
{
    app->usbd = usbd_create(USB_PORT_NUM, USBD_PROCESS_SIZE, USBD_PROCESS_PRIORITY);
    ack(app->usbd, HAL_REQ(HAL_USBD, USBD_REGISTER_HANDLER), 0, 0, 0);

    usbd_register_const_descriptor(app->usbd, &__DEVICE_DESCRIPTOR, 0, 0);
    usbd_register_const_descriptor(app->usbd, &__CONFIGURATION_DESCRIPTOR, 0, 0);
    usbd_register_const_descriptor(app->usbd, &__STRING_WLANGS, 0, 0);
    usbd_register_const_descriptor(app->usbd, &__STRING_MANUFACTURER, 1, 0x0409);
    usbd_register_const_descriptor(app->usbd, &__STRING_PRODUCT, 2, 0x0409);
    usbd_register_const_descriptor(app->usbd, &__STRING_SERIAL, 3, 0x0409);
    usbd_register_const_descriptor(app->usbd, &__STRING_DEFAULT, 4, 0x0409);

    // ??? USBD_IFACE is very mysterious to me
    // ??? should I call this after IPC_OPEN ack?
    rndis_set_link(app->usbd, USBD_IFACE(0,0), ETH_AUTO);

    // ??? as I know, vendor id should be 16 bits, but there we have 24 bits
    // I set 0x0525 vendor id obtained from usb_desc.h, but I don't know the implications, chances are it can be rejected by Windows
    rndis_set_vendor_id(app->usbd, USBD_IFACE(0,0), 0, 0x05, 0x25);
    rndis_set_vendor_description(app->usbd, USBD_IFACE(0,0), "Default");

    // ??? I assume this has to be called, maybe not
    eth_set_mac(KERNEL_HANDLE, 0, &__MAC);

    // ??? maybe it is (should be) automatically called by eth_set_mac
    rndis_set_host_mac(app->usbd, USBD_IFACE(0,0), &__MAC);

    // ??? should I ever call this ack stuff? If yes, when?
    ack(app->usbd, HAL_REQ(HAL_USBD, IPC_OPEN), USB_PORT_NUM, 0, 0);

    app->net.tcpip = tcpip_create(TCPIP_PROCESS_SIZE, TCPIP_PROCESS_PRIORITY, ETH_PHY_ADDRESS);
    ip_set(app->net.tcpip, &__IP);  // assume DHCP is not used
    tcpip_open(app->net.tcpip, KERNEL_HANDLE, ETH_PHY_ADDRESS, ETH_AUTO);

    // ??? assume HTTP process will do the handling its own 80th standard port (I've seen this in webs.c), other ports we don't handle,
    // so we don't need stuff for transport lower-level handling

    // I didn't bother myself with stack size calculation and priority, so I pick whatever comes to my mind
    // by the way, doc has "hs_create" instead of "web_server_create"
    app->net.web = web_server_create(1024, 10);
    web_server_open(app->net.web, 80, app->net.tcpip);

    // this is for web server 
    app->net.io = io_create(512);

    web_server_create_node(app->net.web, WEB_ROOT_NODE, "index.html", WEB_FLAG(WEB_METHOD_GET) | WEB_FLAG(WEB_METHOD_POST));
    app->net.get_count = 0;
    app->net.current_value = 0;
}

int my_atoi(char *str) 
{ 
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) 
        res = res*10 + str[i] - '0'; 
    return res; 
} 

void net_request(APP* app, IPC* ipc)
{
    unsigned int cmd = HAL_ITEM(ipc->cmd);
    switch (cmd)
    {
        // Forms can be sent using get and post, but I cheat a little, asserting that refresh always uses GET, and edit always uses POST
        // the assertion can be broken by the client
        case WEBS_GET:
        case WEBS_POST:
        {
            unsigned int hsession = ipc->param1;
            unsigned int req_size = (unsigned int)ipc->param3;

            // tecnhically I should identify the node, but I know I only have single node
            unsigned int hnode = ipc->param2;         

            if (WEBS_POST == cmd)
            {
                // I don't implement handling, e.g. no parameter, overflow in atoi
                char *numstr = web_server_get_param(app->net.web, hsession, app->net.io, 512, "value");
                app->net.current_value = my_atoi(numstr);
            }

            HS_STACK *desc = io_data(app->net.io);
            char *str = (char*)(desc+1);
            
            app->net.io->data_size = strlen(str) + sizeof(HS_STACK);

            sprintf(str, "<html><body><form action='index.html' method='POST'>value: <input type='number' name='value' value=%d><br>get count: %d<br><input type='submit' value='edit'></form></body></html>\n\r",
                app->net.current_value,
                ++app->net.get_count);

            web_server_write_sync(app->net.web, hsession, WEB_RESPONSE_OK, app->net.io);
        }
        break;

        default:
            error(ERROR_NOT_SUPPORTED);
    }
}
