![GitHub Workflow Status: CI](https://img.shields.io/github/workflow/status/MunifTanjim/llhttp.lua/CI/main?label=CI&style=for-the-badge)
[![Version](https://img.shields.io/luarocks/v/MunifTanjim/llhttp?color=%232c3e67&style=for-the-badge)](https://luarocks.org/modules/MunifTanjim/llhttp)
![License](https://img.shields.io/github/license/MunifTanjim/llhttp.lua?style=for-the-badge)

# llhttp.lua

Lua interface for [nodejs/llhttp](https://github.com/nodejs/llhttp).

## Usage

```lua
local Parser = require("llhttp.parser")
local enum = require("llhttp.enum")

local parser = Parser:new({
  type = enum.type.REQUEST,
})

local request = "GET /test HTTP/1.1\r\nContent-Length: 12\r\n\r\nHello World!\r\n"
local data = { _body_chunks = {} }

local chunk_size = 4
local offset = 1

while offset <= #request do
  local buf = string.sub(request, offset, math.min(offset + chunk_size - 1, #request))

  local err, consumed, pause_cause = parser:execute(buf)

  if err == enum.errno.PAUSED then
    if pause_cause == enum.pause_cause.URL_COMPLETED then
      data.url = parser:pull_url()
    elseif pause_cause == enum.pause_cause.HEADERS_COMPLETED then
      data.headers = parser:pull_headers()
    elseif
      pause_cause == enum.pause_cause.BODY_CHUNK_READY
      or pause_cause == enum.pause_cause.MESSAGE_COMPLETED_WITH_BODY_CHUNK
    then
      data._body_chunks[#data._body_chunks + 1] = parser:pull_body_chunk()
    end

    if
      pause_cause == enum.pause_cause.MESSAGE_COMPLETED_WITH_BODY_CHUNK
      or pause_cause == enum.pause_cause.MESSAGE_COMPLETED
    then
      data.body = table.concat(data._body_chunks)
    end

    parser:resume()
  elseif err == enum.errno.PAUSED_UPGRADE then
    parser:resume_after_upgrade()
  elseif err ~= enum.errno.OK then
    error(parser:get_error_reason())
  end

  offset = offset + consumed
end

print(data.url)
print("=======")
for k, v in pairs(data.headers) do
  print(string.format("%s: %s", k, v))
end
print("=======")
print(data.body)

parser:reset()
```

## License

Licensed under the MIT License. Check the [LICENSE](./LICENSE) file for details.
