/*
@module  rtos
@summary RTOS底层操作库
@version 1.0
@date    2020.03.30
*/
#include "luat_base.h"
#include "luat_sys.h"
#include "luat_msgbus.h"
#include "luat_timer.h"
#include "luat_malloc.h"

#define LUAT_LOG_TAG "rtos"
#include "luat_log.h"

/*
接受并处理底层消息队列.
@api    rtos.receive(timeout)   
@int  超时时长,通常是-1,永久等待
@return msgid          如果是定时器消息,会返回定时器消息id及附加信息, 其他消息由底层决定,不向lua层进行任何保证.
--  本方法通过sys.run()调用, 普通用户不要使用
rtos.receive(-1)
*/
static int l_rtos_receive(lua_State *L) {
    rtos_msg_t msg;
    int re;
    re = luat_msgbus_get(&msg, luaL_checkinteger(L, 1));
    if (!re) {
        //LLOGD("rtos_msg got, invoke it handler=%08X", msg.handler);
        lua_pushlightuserdata(L, (void*)(&msg));
        return msg.handler(L, msg.ptr);
    }
    else {
        //LLOGD("rtos_msg get timeout");
        lua_pushinteger(L, -1);
        return 1;
    }
}

//------------------------------------------------------------------
static int l_timer_handler(lua_State *L, void* ptr) {
    luat_timer_t *timer = (luat_timer_t *)ptr;
    // LLOGD("l_timer_handler id=%ld\n", timer->id);
    lua_pushinteger(L, MSG_TIMER);
    lua_pushinteger(L, timer->id);
    lua_pushinteger(L, timer->repeat);
    //lua_pushinteger(L, timer->timeout);
    if (timer->repeat == 0) {
        // LLOGD("l_timer_handler stop id=%ld\n", timer->id);
        luat_timer_stop(timer);
        luat_heap_free(timer);
    }
    else if (timer->repeat > 0) {
        timer->repeat --;
    }
    return 3;
}

/*
启动一个定时器
@api    rtos.timer_start(id,timeout,_repeat)   
@int  定时器id
@int  超时时长,单位毫秒
@int  重复次数,默认是0
@return id 如果是定时器消息,会返回定时器消息id及附加信息, 其他消息由底层决定,不向lua层进行任何保证.
@usage
-- 用户代码请使用 sys.timerStart
-- 启动一个3秒的循环定时器
rtos.timer_start(10000, 3000, -1)
*/
static int l_rtos_timer_start(lua_State *L) {
    lua_gettop(L);
    size_t id = (size_t)luaL_checkinteger(L, 1) / 1;
    size_t timeout = (size_t)luaL_checkinteger(L, 2);
    int repeat = (size_t)luaL_optinteger(L, 3, 0);
    // LLOGD(("timer id=%ld\n", id);
    // LLOGD(("timer timeout=%ld\n", timeout);
    // LLOGD(("timer repeat=%ld\n", repeat);
    if (timeout < 1) {
        lua_pushinteger(L, 0);
        return 1;
    }
    luat_timer_t *timer = (luat_timer_t*)luat_heap_malloc(sizeof(luat_timer_t));
    timer->id = id;
    timer->timeout = timeout;
    timer->repeat = repeat;
    timer->func = &l_timer_handler;

    int re = luat_timer_start(timer);
    if (re == 0) {
        lua_pushinteger(L, 1);
    }
    else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

/*
关闭并释放一个定时器
@api    rtos.timer_stop(id)   
@int  定时器id
@return nil            无返回值
@usage
-- 用户代码请使用sys.timerStop
rtos.timer_stop(id)
*/
static int l_rtos_timer_stop(lua_State *L) {
    luat_timer_t *timer = NULL;
    if (lua_islightuserdata(L, 1)) {
        timer = (luat_timer_t *)lua_touserdata(L, 1);
    }
    else if (lua_isinteger(L, 1)) {
        timer = luat_timer_get(lua_tointeger(L, 1));
    }
    if (timer != NULL) {
        luat_timer_stop(timer);
        luat_heap_free(timer);
    }
    return 0;
}

/*
设备重启
@api    rtos.reboot()   
@return nil          无返回值
-- 立即重启设备
rtos.reboot()
*/
static int l_rtos_reboot(lua_State *L) {
    luat_os_reboot(luaL_optinteger(L, 1, 0));
    return 0;
}

//-----------------------------------------------------------------

/*
获取固件编译日期
@api    rtos.buildDate()
@return string 固件编译日期
@usage
-- 获取编译日期
local d = rtos.buildDate()
*/
static int l_rtos_build_date(lua_State *L) {
    lua_pushstring(L, __DATE__);
    return 1;
}

/*
获取硬件bsp型号
@api    rtos.bsp()
@return string 硬件bsp型号
@usage
-- 获取硬件bsp型号
local bsp = rtos.bsp()
*/
static int l_rtos_bsp(lua_State *L) {
    lua_pushstring(L, luat_os_bsp());
    return 1;
}

/*
 获取固件版本号
@api    rtos.version()        
@return string  固件版本号,例如"1.0.2"
@usage
-- 读取版本号
local luatos_version = rtos.version()
*/
static int l_rtos_version(lua_State *L) {
    lua_pushstring(L, luat_version_str());
    return 1;
}

/*
进入待机模式(部分设备可用,例如w60x)
@api    rtos.standy(timeout)
@int    休眠时长,单位毫秒     
@return nil  无返回值
@usage
-- 进入待机模式
rtos.standby(5000)
*/
static int l_rtos_standy(lua_State *L) {
    int timeout = luaL_checkinteger(L, 1);
    luat_os_standy(timeout);
    return 0;
}

/*
获取内存信息
@api    rtos.meminfo(type)
@type   "sys"系统内存, "lua"虚拟机内存, 默认为lua虚拟机内存     
@return int 总内存大小,单位字节
@return int 当前使用的内存大小,单位字节
@return int 最大使用的内存大小,单位字节
@usage
-- 打印内存占用
log.info("mem.lua", rtos.meminfo())
log.info("mem.sys", rtos.meminfo("sys"))
*/
static int l_rtos_meminfo(lua_State *L) {
    size_t len = 0;
    size_t total = 0;
    size_t used = 0;
    size_t max_used = 0;
    const char * str = luaL_optlstring(L, 1, "lua", &len);
    if (strcmp("sys", str) == 0) {
        //lua_gc(L, LUA_GCCOLLECT, 0);
        //lua_gc(L, LUA_GCCOLLECT, 0);
        luat_meminfo_sys(&total, &used, &max_used);
    }
    else {
        luat_meminfo_luavm(&total, &used, &max_used);
    }
    lua_pushinteger(L, total);
    lua_pushinteger(L, used);
    lua_pushinteger(L, max_used);
    return 3;
}

/*
返回底层描述信息,格式为 LuatOS_$VERSION_$BSP,可用于OTA升级判断底层信息
@api    rtos.firmware()
@return string 底层描述信息
@usage
-- 打印底层描述信息
log.info("firmware", rtos.firmware())
*/
static int l_rtos_firmware(lua_State *L) {
    lua_pushfstring(L, "LuatOS_%s_%s", luat_version_str(), luat_os_bsp());
    return 1;
}

extern char custom_search_paths[4][24];

/*
设置自定义lua脚本搜索路径,优先级高于内置路径
@api    rtos.setPaths(pathA, pathB, pathC, pathD)
@string 路径A, 例如 "/sdcard/%s.luac",若不传值,将默认为"",另外,最大长度不能超过23字节
@string 路径B, 例如 "/sdcard/%s.lua"
@string 路径C, 例如 "/lfs2/%s.luac"
@string 路径D, 例如 "/lfs2/%s.lua"
@usage
-- 挂载sd卡或者spiflash后
rtos.setPaths("/sdcard/user/%s.luac", "/sdcard/user/%s.lua")
require("sd_user_main") -- 将搜索并加载 /sdcard/user/sd_user_main.luac 和 /sdcard/user/sd_user_main.lua
*/
static int l_rtos_set_paths(lua_State *L) {
    size_t len = 0;
    const char* str = NULL;
    for (size_t i = 0; i < 4; i++)
    {
        if (lua_isstring(L, i +1)) {
            str = luaL_checklstring(L, i+1, &len);
            memcpy(custom_search_paths[i], str, len + 1);
        }
        else {
            custom_search_paths[i][0] = 0x00;
        }
    }
    return 0;
}

static int l_rtos_nop(lua_State *L) {
    return 0;
}
//------------------------------------------------------------------
#include "rotable.h"
static const rotable_Reg reg_rtos[] =
{
    { "timer_start" ,      l_rtos_timer_start, 0},
    { "timer_stop",        l_rtos_timer_stop,  0},
    { "receive",           l_rtos_receive,     0},
    { "reboot",            l_rtos_reboot,      0},
    { "standy",            l_rtos_standy,      0},

    { "buildDate",         l_rtos_build_date,  0},
    { "bsp",               l_rtos_bsp,         0},
    { "version",           l_rtos_version,     0},
    { "meminfo",           l_rtos_meminfo,     0},
    { "firmware",          l_rtos_firmware,    0},
    { "setPaths",          l_rtos_set_paths,   0},
    { "nop",               l_rtos_nop,   0},

    { "INF_TIMEOUT",        NULL,              -1},

    { "MSG_TIMER",          NULL,              MSG_TIMER},
    // { "MSG_GPIO",           NULL,              MSG_GPIO},
    // { "MSG_UART_RX",        NULL,              MSG_UART_RX},
    // { "MSG_UART_TXDONE",    NULL,              MSG_UART_TXDONE},
	{ NULL,                 NULL,              0}
};

LUAMOD_API int luaopen_rtos( lua_State *L ) {
    luat_newlib(L, reg_rtos);
    return 1;
}

LUAT_WEAK const char* luat_version_str(void) {
    #ifdef LUAT_BSP_VERSION
    return LUAT_BSP_VERSION;
    #else
    return LUAT_VERSION;
    #endif
}
