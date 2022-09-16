#include "llhttp.h"
#include <lauxlib.h>
#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

typedef struct lua_llhttp_s lua_llhttp_t;
typedef struct lua_llhttp_data_s lua_llhttp_data_t;
typedef struct lua_llhttp_settings_s lua_llhttp_settings_t;

typedef struct {
  char *buf;
  size_t len;
  size_t size;
} string;

typedef struct {
  string **items;
  size_t len;
  size_t size;
} string_list;

enum lua_llhttp_pause_cause {
  PAUSE_CAUSE_UNKNOWN = 0,
  PAUSE_CAUSE_URL_COMPLETED = 1,
  PAUSE_CAUSE_STATUS_COMPLETED = 2,
  PAUSE_CAUSE_HEADERS_COMPLETED = 3,
  PAUSE_CAUSE_BODY_CHUNK_READY = 4,
  PAUSE_CAUSE_MESSAGE_COMPLETED_WITH_BODY_CHUNK = 5,
  PAUSE_CAUSE_MESSAGE_COMPLETED = 6
};
typedef enum lua_llhttp_pause_cause lua_llhttp_pause_cause_t;

struct lua_llhttp_data_s {
  lua_llhttp_t *llhttp;
  /* table for lua callback */
  int ref;
  string *url;
  string_list *headers;
  string *body;
};

struct lua_llhttp_s {
  llhttp_t parser;
  llhttp_settings_t *settings;
  lua_llhttp_pause_cause_t pause_cause;
  size_t body_chunk_size_threshold;
  struct {
    int on_message_begin;
    int on_url;
    int on_status;
    int on_header_field;
    int on_header_value;
    int on_headers_complete;
    int on_body;
    int on_message_complete;
    int on_url_complete;
    int on_status_complete;
    int on_header_field_complete;
    int on_header_value_complete;
    int on_chunk_header;
    int on_chunk_complete;
    int on_reset;
  } cb_ref;
  lua_llhttp_data_t data;
  lua_State *L;
  int ref;
};

string *_lua_llhttp__string_new() {
  string *str = malloc(sizeof *str);
  str->buf = malloc(16 + 1);
  str->len = 0;
  str->size = 16;
  str->buf[str->len] = '\0';
  return str;
}

void _lua_llhttp__string_free(string *str) {
  free(str->buf);
  free(str);
}

string *_lua_llhttp__string_reset(string *str) {
  str->len = 0;
  str->buf[str->len] = '\0';
  return str;
}

// returns `0` if successful, `-1` otherwise.
int _lua_llhttp__string_append(string *str, const char *buf, size_t len) {
  size_t new_len = str->len + len;
  if (str->size < new_len) {
    size_t new_size = str->size * 2;
    while (new_size < new_len) {
      new_size = new_size * 2;
    }

    void *new_buf = realloc(str->buf, new_size + 1);
    if (new_buf == NULL) {
      return -1;
    }

    str->buf = new_buf;
    str->size = new_size;
  }

  memcpy(str->buf + str->len, buf, len);
  str->len = new_len;
  str->buf[str->len] = '\0';

  return 0;
}

string_list *_lua_llhttp__string_list_new() {
  string_list *list = malloc(sizeof *list);
  list->items = malloc(16 * sizeof(list->items));
  list->len = 0;
  list->size = 16;
  return list;
}

void _lua_llhttp__string_list_free(string_list *list) {
  for (int i = 0; i < list->len; i++) {
    _lua_llhttp__string_free(list->items[i]);
  }
  free(list->items);
  free(list);
}

string_list *_lua_llhttp__string_list_reset(string_list *list) {
  _lua_llhttp__string_list_free(list);
  list = _lua_llhttp__string_list_new();
  return list;
}

// returns `0` if successful, `-1` otherwise.
int _lua_llhttp__string_list_add(string_list *list) {
  if (list->size == list->len + 1) {
    void *new_items =
        realloc(list->items, 2 * list->size * sizeof(list->items));
    if (new_items == NULL) {
      return -1;
    }

    list->items = new_items;
    list->size = 2 * list->size;
  }

  list->items[list->len++] = _lua_llhttp__string_new();

  return 0;
}

#define LUA_LLHTTP_META "llhttp.core.lua_llhttp"

#define LUA_LLHTTP_CALLBACK_MAYBE(NAME)                                        \
  if (data->llhttp->cb_ref.NAME) {                                             \
    lua_rawgeti(data->llhttp->L, LUA_REGISTRYINDEX,                            \
                data->llhttp->cb_ref.NAME); /* [cb] */                         \
    lua_rawgeti(data->llhttp->L, LUA_REGISTRYINDEX,                            \
                data->llhttp->ref);  /* [cb, meta(llhttp)] */                  \
    lua_call(data->llhttp->L, 1, 1); /* [err] */                               \
    err = luaL_checkint(data->llhttp->L, -1);                                  \
  }

#define DEFINE_LUA_LLHTTP_CALLBACK(NAME)                                       \
  int lua_##NAME(llhttp_t *parser) {                                           \
    int err = HPE_OK;                                                          \
    const lua_llhttp_data_t *data = (const lua_llhttp_data_t *)parser->data;   \
    LUA_LLHTTP_CALLBACK_MAYBE(NAME);                                           \
    return err;                                                                \
  }

#define LUA_LLHTTP_DATA_CALLBACK_MAYBE(NAME)                                   \
  if (data->llhttp->cb_ref.NAME) {                                             \
    lua_rawgeti(data->llhttp->L, LUA_REGISTRYINDEX,                            \
                data->llhttp->cb_ref.NAME); /* [cb] */                         \
    lua_rawgeti(data->llhttp->L, LUA_REGISTRYINDEX,                            \
                data->llhttp->ref);      /* [cb, meta(llhttp)] */              \
    lua_pushstring(data->llhttp->L, at); /* [cb, meta(llhttp), at] */          \
    lua_pushinteger(data->llhttp->L,                                           \
                    length);         /* [cb, meta(llhttp), at, length] */      \
    lua_call(data->llhttp->L, 3, 1); /* [err] */                               \
    err = luaL_checkint(data->llhttp->L, -1);                                  \
  }

#define DEFINE_LUA_LLHTTP_DATA_CALLBACK(NAME)                                  \
  int lua_##NAME(llhttp_t *parser, const char *at, size_t length) {            \
    int err = HPE_OK;                                                          \
    const lua_llhttp_data_t *data = (const lua_llhttp_data_t *)parser->data;   \
    LUA_LLHTTP_DATA_CALLBACK_MAYBE(NAME);                                      \
    return err;                                                                \
  }

DEFINE_LUA_LLHTTP_CALLBACK(on_message_begin);
DEFINE_LUA_LLHTTP_CALLBACK(on_headers_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_message_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_url_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_status_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_header_field_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_header_value_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_chunk_header);
DEFINE_LUA_LLHTTP_CALLBACK(on_chunk_complete);
DEFINE_LUA_LLHTTP_CALLBACK(on_reset);

DEFINE_LUA_LLHTTP_DATA_CALLBACK(on_url);
DEFINE_LUA_LLHTTP_DATA_CALLBACK(on_status);
DEFINE_LUA_LLHTTP_DATA_CALLBACK(on_header_field);
DEFINE_LUA_LLHTTP_DATA_CALLBACK(on_header_value);
DEFINE_LUA_LLHTTP_DATA_CALLBACK(on_body);

#define LUA_LLHTTP_HANDLE_C_CB_OOM(CB_NAME, OOM_ERRNO, FUNCTION_CALL)          \
  if (OOM_ERRNO == FUNCTION_CALL) {                                            \
    return luaL_error(data->llhttp->L, "out of memory (at %s)", CB_NAME);      \
  }

int c_on_message_begin(llhttp_t *parser) { return 0; }

// request
int c_on_url(llhttp_t *parser, const char *at, size_t length) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  LUA_LLHTTP_HANDLE_C_CB_OOM("on_url", -1,
                             _lua_llhttp__string_append(data->url, at, length));
  return 0;
}

// response
int c_on_status(llhttp_t *parser, const char *at, size_t length) { return 0; }

int c_on_header_field(llhttp_t *parser, const char *at, size_t length) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  if (data->headers->len % 2 == 0) {
    LUA_LLHTTP_HANDLE_C_CB_OOM("on_header_field", -1,
                               _lua_llhttp__string_list_add(data->headers));
  }
  LUA_LLHTTP_HANDLE_C_CB_OOM(
      "on_header_field", -1,
      _lua_llhttp__string_append(data->headers->items[data->headers->len - 1],
                                 at, length));
  return 0;
}

int c_on_header_value(llhttp_t *parser, const char *at, size_t length) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  if (data->headers->len % 2 == 1) {
    LUA_LLHTTP_HANDLE_C_CB_OOM("on_header_value", -1,
                               _lua_llhttp__string_list_add(data->headers));
  }
  LUA_LLHTTP_HANDLE_C_CB_OOM(
      "on_header_value", -1,
      _lua_llhttp__string_append(data->headers->items[data->headers->len - 1],
                                 at, length));
  return 0;
}

int c_on_headers_complete(llhttp_t *parser) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  data->llhttp->pause_cause = PAUSE_CAUSE_HEADERS_COMPLETED;
  return HPE_PAUSED;
}

int c_on_body(llhttp_t *parser, const char *at, size_t length) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  LUA_LLHTTP_HANDLE_C_CB_OOM(
      "on_body", -1, _lua_llhttp__string_append(data->body, at, length));
  if (parser->content_length > 0 &&
      data->llhttp->body_chunk_size_threshold <= data->body->len) {
    data->llhttp->pause_cause = PAUSE_CAUSE_BODY_CHUNK_READY;
    return HPE_PAUSED;
  }
  return 0;
}

int c_on_message_complete(llhttp_t *parser) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  data->llhttp->pause_cause =
      data->body->len ? PAUSE_CAUSE_MESSAGE_COMPLETED_WITH_BODY_CHUNK
                      : PAUSE_CAUSE_MESSAGE_COMPLETED;
  return HPE_PAUSED;
}

// request
int c_on_url_complete(llhttp_t *parser) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  data->llhttp->pause_cause = PAUSE_CAUSE_URL_COMPLETED;
  return HPE_PAUSED;
}

// response
int c_on_status_complete(llhttp_t *parser) {
  lua_llhttp_data_t *data = (lua_llhttp_data_t *)parser->data;
  data->llhttp->pause_cause = PAUSE_CAUSE_STATUS_COMPLETED;
  return HPE_PAUSED;
}

int c_on_header_field_complete(llhttp_t *parser) { return 0; }

int c_on_header_value_complete(llhttp_t *parser) { return 0; }

int c_on_chunk_header(llhttp_t *parser) { return 0; }

int c_on_chunk_complete(llhttp_t *parser) { return 0; }

int c_on_reset(llhttp_t *parser) { return 0; }

llhttp_settings_t lua_llhttp_c_settings;
llhttp_settings_t lua_llhttp_lua_settings;

#define REGISTER_LUA_LLHTTP_CALLBACK(NAME)                                     \
  lua_getfield(L, 2, #NAME); /* [cb?] */                                       \
  if (lua_isfunction(L, -1)) {                                                 \
    has_lua_callback = 1;                                                      \
    lua_llhttp->cb_ref.NAME = luaL_ref(L, LUA_REGISTRYINDEX); /* [] */         \
  } else {                                                                     \
    lua_pop(L, 1); /* [] */                                                    \
  }

#define UNREGISTER_LUA_LLHTTP_CALLBACK(NAME)                                   \
  if (lua_llhttp->cb_ref.NAME) {                                               \
    luaL_unref(L, LUA_REGISTRYINDEX, lua_llhttp->cb_ref.NAME);                 \
  }

#define SET_BOOLEAN_FLAG(NAME)                                                 \
  lua_getfield(L, 2, #NAME); /* [value] */                                     \
  if (lua_isboolean(L, -1)) {                                                  \
    llhttp_set_##NAME(&lua_llhttp->parser, lua_toboolean(L, -1));              \
  }                                                                            \
  lua_pop(L, 1); /* [] */

#define GET_LUA_LLHTTP_SELF_PTR()                                              \
  lua_llhttp_t *lua_llhttp = lua_llhttp_checkself(L);                          \
  lua_llhttp->L = L;

#define DEFINE_LUA_LLHTTP_RET_NONE(NAME)                                       \
  int lua_llhttp_##NAME(lua_State *L) {                                        \
    GET_LUA_LLHTTP_SELF_PTR();                                                 \
    llhttp_##NAME(&lua_llhttp->parser);                                        \
    return 0;                                                                  \
  }

#define DEFINE_LUA_LLHTTP_RET_INTEGER(NAME)                                    \
  int lua_llhttp_##NAME(lua_State *L) {                                        \
    GET_LUA_LLHTTP_SELF_PTR();                                                 \
    int ret = llhttp_##NAME(&lua_llhttp->parser);                              \
    lua_pushinteger(L, ret); /* [ret] */                                       \
    return 1;                                                                  \
  }

#define DEFINE_LUA_LLHTTP_RET_BOOLEAN(NAME)                                    \
  int lua_llhttp_##NAME(lua_State *L) {                                        \
    GET_LUA_LLHTTP_SELF_PTR();                                                 \
    int bool = llhttp_##NAME(&lua_llhttp->parser);                             \
    lua_pushboolean(L, bool); /* [bool] */                                     \
    return 1;                                                                  \
  }

#define DEFINE_LUA_LLHTTP_SET_LENIENT_OPTION(NAME)                             \
  int lua_llhttp_set_lenient_##NAME(lua_State *L) {                            \
    GET_LUA_LLHTTP_SELF_PTR();                                                 \
    int enabled = luaL_checkinteger(L, 2);                                     \
    llhttp_set_lenient_##NAME(&lua_llhttp->parser, enabled);                   \
    return 0;                                                                  \
  }

int lua_llhttp_create_parser(lua_State *L) {
  int n = lua_gettop(L);
  int has_custom_settings = n == 2;

  if (n < 1) {
    return luaL_error(L, "missing required param: type");
  }

  llhttp_type_t llhttp_type = luaL_checkint(L, 1);

  if (has_custom_settings && !lua_istable(L, 2)) {
    return luaL_error(L, "invalid settings");
  }

  lua_llhttp_t *lua_llhttp =
      lua_newuserdata(L, sizeof(*lua_llhttp));      // [llhttp]
  luaL_getmetatable(L, LUA_LLHTTP_META);            // [llhttp, meta]
  lua_setmetatable(L, -2);                          // [meta(llhttp)]
  lua_llhttp->ref = luaL_ref(L, LUA_REGISTRYINDEX); // []

  int has_lua_callback = 0;

  if (has_custom_settings) {
    REGISTER_LUA_LLHTTP_CALLBACK(on_message_begin);
    REGISTER_LUA_LLHTTP_CALLBACK(on_url);
    REGISTER_LUA_LLHTTP_CALLBACK(on_status);
    REGISTER_LUA_LLHTTP_CALLBACK(on_header_field);
    REGISTER_LUA_LLHTTP_CALLBACK(on_header_value);
    REGISTER_LUA_LLHTTP_CALLBACK(on_headers_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_message_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_body);
    REGISTER_LUA_LLHTTP_CALLBACK(on_url_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_status_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_header_field_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_header_value_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_chunk_header);
    REGISTER_LUA_LLHTTP_CALLBACK(on_chunk_complete);
    REGISTER_LUA_LLHTTP_CALLBACK(on_reset);

    SET_BOOLEAN_FLAG(lenient_headers);
    SET_BOOLEAN_FLAG(lenient_chunked_length);
    SET_BOOLEAN_FLAG(lenient_keep_alive);
    SET_BOOLEAN_FLAG(lenient_transfer_encoding);

    lua_getfield(L, 2, "body_chunk_size_threshold"); /* [value] */
    lua_llhttp->body_chunk_size_threshold = luaL_optint(L, -1, 0);
    lua_pop(L, 1); /* [] */
  }

  if (has_lua_callback) {
    lua_llhttp->settings = &lua_llhttp_lua_settings;

    lua_newtable(L);                                       // [data]
    lua_newtable(L);                                       // [data, {}]
    lua_setfield(L, -2, "headers");                        // [data]
    lua_llhttp->data.ref = luaL_ref(L, LUA_REGISTRYINDEX); // []
  } else {
    lua_llhttp->settings = &lua_llhttp_c_settings;

    lua_llhttp->data.url = _lua_llhttp__string_new();
    lua_llhttp->data.headers = _lua_llhttp__string_list_new();
    lua_llhttp->data.body = _lua_llhttp__string_new();

    lua_llhttp->data.ref = 0;
  }

  llhttp_init(&lua_llhttp->parser, llhttp_type, lua_llhttp->settings);

  lua_llhttp->L = L;
  lua_llhttp->data.llhttp = lua_llhttp;
  lua_llhttp->parser.data = &lua_llhttp->data;

  lua_rawgeti(L, LUA_REGISTRYINDEX, lua_llhttp->ref); // [meta(llhttp)]

  return 1;
}

lua_llhttp_t *lua_llhttp_checkself(lua_State *L) {
  return luaL_checkudata(L, 1, LUA_LLHTTP_META);
}

DEFINE_LUA_LLHTTP_RET_INTEGER(get_type);
DEFINE_LUA_LLHTTP_RET_INTEGER(get_http_major);
DEFINE_LUA_LLHTTP_RET_INTEGER(get_http_minor);
// request
DEFINE_LUA_LLHTTP_RET_INTEGER(get_method);
// response
DEFINE_LUA_LLHTTP_RET_INTEGER(get_status_code);
DEFINE_LUA_LLHTTP_RET_BOOLEAN(get_upgrade);

int lua_llhttp_reset(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();

  llhttp_reset(&lua_llhttp->parser);

  if (lua_llhttp->data.ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_llhttp->data.ref); // [data]
    lua_pushnil(L);                                          // [data, nil]
    lua_setfield(L, -2, "url");                              // [data]
    lua_newtable(L);                                         // [data,  {}]
    lua_setfield(L, -2, "headers");                          // [data]
    lua_pushnil(L);                                          // [data, nil]
    lua_setfield(L, -2, "body");                             // [data]
  } else {
    lua_llhttp->data.url = _lua_llhttp__string_reset(lua_llhttp->data.url);
    lua_llhttp->data.headers =
        _lua_llhttp__string_list_reset(lua_llhttp->data.headers);
    lua_llhttp->data.body = _lua_llhttp__string_reset(lua_llhttp->data.body);
  }

  return 0;
}

int lua_llhttp_execute(lua_State *L) {
  if (lua_gettop(L) != 2) {
    return luaL_error(L, "incorrect number of params");
  }

  GET_LUA_LLHTTP_SELF_PTR();

  size_t buf_len;
  const char *buf = luaL_checklstring(L, 2, &buf_len);

  enum llhttp_errno err = llhttp_execute(&lua_llhttp->parser, buf, buf_len);

  int consumed = 0;
  lua_llhttp_pause_cause_t pause_cause = PAUSE_CAUSE_UNKNOWN;
  switch (err) {
  case HPE_OK:
    consumed = buf_len;
    break;
  case HPE_PAUSED:
    pause_cause = lua_llhttp->pause_cause;
    lua_llhttp->pause_cause = PAUSE_CAUSE_UNKNOWN;
  case HPE_PAUSED_UPGRADE:
  case HPE_PAUSED_H2_UPGRADE:
    consumed = lua_llhttp->parser.error_pos - buf;
    break;
  default:
    break;
  }

  lua_pushinteger(L, err);      // [err]
  lua_pushinteger(L, consumed); // [err, consumed]
  if (err == HPE_PAUSED) {
    lua_pushinteger(L, pause_cause); // [err, consumed, pause_cause]
  } else {
    lua_pushnil(L); // [err, consumed, nil]
  }

  return 3;
}

DEFINE_LUA_LLHTTP_RET_INTEGER(finish);

DEFINE_LUA_LLHTTP_RET_BOOLEAN(message_needs_eof);
DEFINE_LUA_LLHTTP_RET_BOOLEAN(should_keep_alive);

DEFINE_LUA_LLHTTP_RET_NONE(pause);
DEFINE_LUA_LLHTTP_RET_NONE(resume);
DEFINE_LUA_LLHTTP_RET_NONE(resume_after_upgrade);

DEFINE_LUA_LLHTTP_RET_INTEGER(get_errno);

int lua_llhttp_get_error_reason(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();
  const char *error_reason = llhttp_get_error_reason(&lua_llhttp->parser);
  lua_pushstring(L, error_reason); // [error_reason]
  return 1;
}

int lua_llhttp_set_error_reason(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();
  const char *error_reason = luaL_checkstring(L, 2);
  llhttp_set_error_reason(&lua_llhttp->parser, error_reason);
  return 0;
}

DEFINE_LUA_LLHTTP_SET_LENIENT_OPTION(headers);
DEFINE_LUA_LLHTTP_SET_LENIENT_OPTION(chunked_length);
DEFINE_LUA_LLHTTP_SET_LENIENT_OPTION(keep_alive);
DEFINE_LUA_LLHTTP_SET_LENIENT_OPTION(transfer_encoding);

int lua_llhttp_pull_url(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();

  if (lua_llhttp->data.ref) {
    return 0;
  }

  lua_pushlstring(L, lua_llhttp->data.url->buf,
                  lua_llhttp->data.url->len); // [url]

  lua_llhttp->data.url = _lua_llhttp__string_reset(lua_llhttp->data.url);

  return 1;
}

int lua_llhttp_pull_headers(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();

  if (lua_llhttp->data.ref) {
    return 0;
  }

  lua_newtable(L); // [{}]

  if (!lua_llhttp->data.headers->len) {
    return 1;
  }

  for (int i = 0; i < lua_llhttp->data.headers->len; i = i + 2) {
    lua_pushlstring(L, lua_llhttp->data.headers->items[i + 1]->buf,
                    lua_llhttp->data.headers->items[i + 1]->len); // [{}, value]
    lua_setfield(L, -2,
                 lua_llhttp->data.headers->items[i]->buf); // [{}]
  }

  lua_llhttp->data.headers =
      _lua_llhttp__string_list_reset(lua_llhttp->data.headers);

  return 1;
}

int lua_llhttp_pull_body_chunk(lua_State *L) {
  GET_LUA_LLHTTP_SELF_PTR();

  if (lua_llhttp->data.ref) {
    return 0;
  }

  lua_pushlstring(L, lua_llhttp->data.body->buf,
                  lua_llhttp->data.body->len); // [body]

  lua_llhttp->data.body = _lua_llhttp__string_reset(lua_llhttp->data.body);

  return 1;
}

int lua_llhttp__index(lua_State *L) {
  lua_llhttp_t *lua_llhttp = lua_llhttp_checkself(L);
  const char *key = luaL_checkstring(L, 2);

  if (lua_llhttp->data.ref != 0 && strcmp(key, "data") == 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_llhttp->data.ref); // [{}]
    return 1;
  }

  luaL_getmetatable(L, LUA_LLHTTP_META); // [meta]
  lua_getfield(L, -1, key);              // [meta, value?]
  return 1;
}

int lua_llhttp__gc(lua_State *L) {
  lua_llhttp_t *lua_llhttp = lua_llhttp_checkself(L);
  luaL_unref(L, LUA_REGISTRYINDEX, lua_llhttp->ref);
  if (lua_llhttp->data.ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, lua_llhttp->data.ref);

    UNREGISTER_LUA_LLHTTP_CALLBACK(on_message_begin);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_url);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_status);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_header_field);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_header_value);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_headers_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_message_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_body);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_url_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_status_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_header_field_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_header_value_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_chunk_header);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_chunk_complete);
    UNREGISTER_LUA_LLHTTP_CALLBACK(on_reset);
  } else {
    _lua_llhttp__string_free(lua_llhttp->data.url);
    _lua_llhttp__string_list_free(lua_llhttp->data.headers);
    _lua_llhttp__string_free(lua_llhttp->data.body);
  }
  return 0;
}

#define DEFINE_LUA_LLHTTP_GET_NAME(NAME)                                       \
  int lua_llhttp_get_##NAME##_name(lua_State *L) {                             \
    llhttp_##NAME##_t NAME = luaL_checkinteger(L, 1);                          \
    lua_pushstring(L, llhttp_##NAME##_name(NAME)); /* [name] */                \
    return 1;                                                                  \
  }

DEFINE_LUA_LLHTTP_GET_NAME(errno);
DEFINE_LUA_LLHTTP_GET_NAME(method);
DEFINE_LUA_LLHTTP_GET_NAME(status);

static const luaL_reg lua_llhttp_meta[] = {
    {"__index", lua_llhttp__index},
    {"__gc", lua_llhttp__gc},
    {"get_type", lua_llhttp_get_type},
    {"get_http_major", lua_llhttp_get_http_major},
    {"get_http_minor", lua_llhttp_get_http_minor},
    {"get_method", lua_llhttp_get_method},
    {"get_status_code", lua_llhttp_get_status_code},
    {"get_upgrade", lua_llhttp_get_upgrade},
    {"reset", lua_llhttp_reset},
    {"execute", lua_llhttp_execute},
    {"finish", lua_llhttp_finish},
    {"message_needs_eof", lua_llhttp_message_needs_eof},
    {"should_keep_alive", lua_llhttp_should_keep_alive},
    {"pause", lua_llhttp_pause},
    {"resume", lua_llhttp_resume},
    {"resume_after_upgrade", lua_llhttp_resume_after_upgrade},
    {"get_errno", lua_llhttp_get_errno},
    {"get_error_reason", lua_llhttp_get_error_reason},
    {"set_error_reason", lua_llhttp_set_error_reason},
    {"set_lenient_headers", lua_llhttp_set_lenient_headers},
    {"set_lenient_chunked_length", lua_llhttp_set_lenient_chunked_length},
    {"set_lenient_keep_alive", lua_llhttp_set_lenient_keep_alive},
    {"set_lenient_transfer_encoding", lua_llhttp_set_lenient_transfer_encoding},
    {"pull_url", lua_llhttp_pull_url},
    {"pull_headers", lua_llhttp_pull_headers},
    {"pull_body_chunk", lua_llhttp_pull_body_chunk},
    {NULL, NULL}};

static const luaL_reg lua_llhttp_funcs[] = {
    {"create_parser", lua_llhttp_create_parser},
    {"get_errno_name", lua_llhttp_get_errno_name},
    {"get_method_name", lua_llhttp_get_method_name},
    {"get_status_name", lua_llhttp_get_status_name},
    {NULL, NULL}};

#define LUA_LLHTTP_SETTINGS_INIT(LANG)                                         \
  llhttp_settings_init(&lua_llhttp_##LANG##_settings);                         \
  lua_llhttp_##LANG##_settings.on_url = LANG##_on_url;                         \
  lua_llhttp_##LANG##_settings.on_message_begin = LANG##_on_message_begin;     \
  lua_llhttp_##LANG##_settings.on_status = LANG##_on_status;                   \
  lua_llhttp_##LANG##_settings.on_header_field = LANG##_on_header_field;       \
  lua_llhttp_##LANG##_settings.on_header_value = LANG##_on_header_value;       \
  lua_llhttp_##LANG##_settings.on_headers_complete =                           \
      LANG##_on_headers_complete;                                              \
  lua_llhttp_##LANG##_settings.on_body = LANG##_on_body;                       \
  lua_llhttp_##LANG##_settings.on_message_complete =                           \
      LANG##_on_message_complete;                                              \
  lua_llhttp_##LANG##_settings.on_url_complete = LANG##_on_url_complete;       \
  lua_llhttp_##LANG##_settings.on_status_complete = LANG##_on_status_complete; \
  lua_llhttp_##LANG##_settings.on_header_field_complete =                      \
      LANG##_on_header_field_complete;                                         \
  lua_llhttp_##LANG##_settings.on_header_value_complete =                      \
      LANG##_on_header_value_complete;                                         \
  lua_llhttp_##LANG##_settings.on_chunk_header = LANG##_on_chunk_header;       \
  lua_llhttp_##LANG##_settings.on_chunk_complete = LANG##_on_chunk_complete;   \
  lua_llhttp_##LANG##_settings.on_reset = LANG##_on_reset;

int luaopen_llhttp_core(lua_State *L) {
  LUA_LLHTTP_SETTINGS_INIT(c);
  LUA_LLHTTP_SETTINGS_INIT(lua);

  if (luaL_newmetatable(L, LUA_LLHTTP_META)) { // [meta]
    luaL_register(L, NULL, lua_llhttp_meta);
    lua_pop(L, 1); // []
  }

  lua_newtable(L); // [{}]
  luaL_register(L, NULL, lua_llhttp_funcs);

  return 1;
}
