#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>

#include <wimax/WiMaxAPI.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LUA_WIMAX_NAME "wimax"
#define MT_WIMAX "wimax mt"


struct lwimax_ud {
  WIMAX_API_DEVICE_ID dev_id;
};


#define print_error(fmt, ...){ \
  char err[256]; \
  int pos = snprintf (err, 256, "[%s:%d in %s] ", __FILE__, __LINE__, __func__); \
  snprintf (err+pos, 256-pos, fmt, ##__VA_ARGS__); \
  lua_pushstring (L, err); \
  lua_error (L); \
}

/*
 * privilege (integer)
 * returns wimax object (userdata)
 */
static int
lwimax_open (lua_State *L)
{
  struct lwimax_ud *ud;
  lua_Integer privilege = luaL_checkinteger (L, 1);

  if (privilege != WIMAX_API_PRIVILEGE_READ_ONLY)
    print_error ("only PRIVILEGE_READ_ONLY is supported at this time");

  ud = lua_newuserdata (L, sizeof *ud);
  if (NULL == ud)
    print_error ("out of memory");

  luaL_getmetatable (L, MT_WIMAX);
  lua_setmetatable (L, -2);

  ud->dev_id.privilege = privilege;
  if (WIMAX_API_RET_SUCCESS != WiMaxAPIOpen (&ud->dev_id))
    print_error ("failed to open wimax. try with sudo?");

  return 1;
}

/*
 * wimax object (userdata)
 */
static int
lwimax_close (lua_State *L)
{
  struct lwimax_ud *ud = luaL_checkudata (L, 1, MT_WIMAX);

  if (WIMAX_API_RET_SUCCESS != WiMaxAPIClose (&ud->dev_id))
    print_error ("unable to close wimax api");

  return 0;
}

/*
 * wimax object (userdata)
 * returns table of dev_index -> dev_name
 */
static int
lwimax_get_device_list (lua_State *L)
{
  struct lwimax_ud *ud = luaL_checkudata (L, 1, MT_WIMAX);
  WIMAX_API_HW_DEVICE_ID *hw_dev_ids = NULL;
  uint32_t size = 0;
  size_t i;

  if (WIMAX_API_RET_BUFFER_SIZE_TOO_SMALL != GetListDevice (&ud->dev_id, hw_dev_ids, &size))
    print_error ("no suitable device?");

  hw_dev_ids = malloc ((sizeof *hw_dev_ids) * size);
  if (NULL == hw_dev_ids)
    print_error ("out of memory");

  if (WIMAX_API_RET_SUCCESS != GetListDevice (&ud->dev_id, hw_dev_ids, &size))
    print_error ("removing devices too quickly?");

  lua_createtable (L, 0, 0);
  for (i = 0; i < size; i++) {
    lua_pushinteger (L, hw_dev_ids[i].deviceIndex + 1);
    lua_pushstring (L, hw_dev_ids[i].deviceName);
    lua_settable (L, -3);
  }

  free (hw_dev_ids);

  return 1;
}

/*
 * device index (integer)
 */
static int
lwimax_device_open (lua_State *L)
{
  struct lwimax_ud *ud = luaL_checkudata (L, 1, MT_WIMAX);
  lua_Integer device_index = luaL_checkinteger (L, 2) - 1;

  ud->dev_id.deviceIndex = device_index;
  if (WIMAX_API_RET_SUCCESS != WiMaxDeviceOpen (&ud->dev_id))
    print_error ("unable to open the device");

  return 0;
}

static int
lwimax_device_close (lua_State *L)
{
  struct lwimax_ud *ud = luaL_checkudata (L, 1, MT_WIMAX);

  if (WIMAX_API_RET_SUCCESS != WiMaxDeviceClose (&ud->dev_id))
    print_error ("unable to close the device");

  return 0;
}

static int
convert_rssi (uint8_t rssi)
{
  return (int) rssi - 123;
}

static int
convert_cinr (uint8_t cinr)
{
  return (int) cinr - 10;
}

static int
convert_txpower (uint8_t power)
{
  return ((double) power) * 0.5 - 84;
}

#define SET_TABLE_INT(key, value) lua_pushliteral (L, key); lua_pushinteger (L, value); lua_settable (L, -3)

/*
 * wimax object (userdata)
 * returns link info (table)
 */
static int
lwimax_get_link_status (lua_State *L)
{
  struct lwimax_ud *ud = luaL_checkudata (L, 1, MT_WIMAX);
  WIMAX_API_LINK_STATUS_INFO link_status;

  if (WIMAX_API_RET_SUCCESS != GetLinkStatus (&ud->dev_id, &link_status)) {
    lua_pushnil (L);
    return 1;
  }

  lua_createtable (L, 0, 0);
  SET_TABLE_INT ("freq", link_status.centerFrequency);
  SET_TABLE_INT ("rssi", convert_rssi (link_status.RSSI));
  SET_TABLE_INT ("cinr", convert_cinr (link_status.CINR));
  SET_TABLE_INT ("txpwr", convert_txpower (link_status.txPWR));

  lua_pushliteral (L, "bs_id");
  {
    int i;
    lua_createtable (L, 0, 0);
    for (i = 0; i < 6; i++) {
      lua_pushinteger (L, i + 1);
      lua_pushinteger (L, link_status.bsId[i]);
      lua_settable (L, -3);
    }
  }
  lua_settable (L, -3);

  return 1;
}

static const luaL_Reg wimaxlib[] = {
  {"open", lwimax_open},
  {NULL, NULL}
};

static const luaL_Reg wimax_methods[] = {
  {"__gc", lwimax_close},
  {"close", lwimax_close},
  {"get_device_list", lwimax_get_device_list},
  {"device_open", lwimax_device_open},
  {"device_close", lwimax_device_close},
  {"get_link_status", lwimax_get_link_status},
  {NULL, NULL}
};

#define SET_WIMAX_CONST_INT(s) lua_pushinteger (L, WIMAX_API_##s); lua_setfield (L, -2, #s)

LUALIB_API int luaopen_wimax (lua_State *L)
{
  luaL_register (L, LUA_WIMAX_NAME, wimaxlib);

  SET_WIMAX_CONST_INT (PRIVILEGE_READ_ONLY);

  if (!luaL_newmetatable (L, MT_WIMAX))
    print_error ("unable to create gps metatable");
  lua_createtable (L, 0, sizeof (wimax_methods) / sizeof (luaL_Reg) - 1);
  luaL_register (L, NULL, wimax_methods);
  lua_setfield (L, -2, "__index");

  return 1;
}
