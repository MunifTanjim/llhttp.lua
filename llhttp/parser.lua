local LLHTTP = require("llhttp.core")
local enum = require("llhttp.enum")

---@class LLHTTPParser
---@field llhttp table
local Parser = {}
Parser.__index = Parser

---@return LLHTTPParser parser
local function init(class, settings)
  local self = {
    _completed_method = false,
    _completed_url = false,
    _completed_version = false,
    _completed_status = false,
    _completed_headers = false,
  }

  if settings.type ~= enum.type.BOTH then
    self._type = settings.type
  end

  self.llhttp = LLHTTP.create_parser(settings.type, settings)

  setmetatable(self, class)

  return self
end

function Parser:new(settings)
  return init(self, settings)
end

function Parser:get_type()
  if not self._type then
    local llhttp_type = self.llhttp:get_type()
    if llhttp_type ~= enum.type.BOTH then
      self._type = llhttp_type
    end
  end

  return self._type
end

---@return number http_version
function Parser:get_http_version()
  if not self._completed_version then
    error("http_version not parsed yet")
  end

  if not self._http_version then
    local major = self.llhttp:get_http_major()
    local minor = self.llhttp:get_http_minor()
    self._http_version = major + minor * 0.1
  end

  return self._http_version
end

function Parser:get_method()
  if self:get_type() ~= enum.type.REQUEST then
    error("only available for request parser")
  end

  if not self._completed_method then
    error("method not parsed yet")
  end

  if not self._method then
    self._method = self.llhttp:get_method()
  end

  return self._method
end

---@return integer status_code
function Parser:get_status_code()
  if self:get_type() ~= enum.type.RESPONSE then
    error("only available for response parser")
  end

  if not self._completed_status then
    error("status_code not parsed yet")
  end

  if not self._status_code then
    self._status_code = self.llhttp:get_status_code()
  end

  return self._status_code
end

---@return boolean upgrade
function Parser:get_upgrade()
  return self.llhttp:get_upgrade()
end

---@return nil
function Parser:reset()
  self._completed_method = false
  self._completed_url = false
  self._completed_version = false
  self._completed_status = false
  self._completed_headers = false

  self._http_version = nil
  self._method = nil
  self._status_code = nil

  return self.llhttp:reset()
end

---@param buf string
---@return integer errno `llhttp_errno`
---@return integer consumed_bytes
---@return integer pause_cause|nil
function Parser:execute(buf)
  local err, consumed, pause_cause = self.llhttp:execute(buf)
  if err == enum.errno.PAUSED then
    if pause_cause == enum.pause_cause.METHOD_COMPLETED then
      self._completed_method = true
    elseif pause_cause == enum.pause_cause.URL_COMPLETED then
      self._completed_url = true
    elseif pause_cause == enum.pause_cause.VERSION_COMPLETED then
      self._completed_version = true
    elseif pause_cause == enum.pause_cause.STATUS_COMPLETED then
      self._completed_status = true
    elseif pause_cause == enum.pause_cause.HEADERS_COMPLETED then
      self._completed_headers = true
    end
  end
  return err, consumed, pause_cause
end

---@return integer errno `llhttp_errno`
function Parser:finish()
  return self.llhttp:finish()
end

---@return boolean message_needs_eof
function Parser:message_needs_eof()
  if self:get_type() ~= enum.type.RESPONSE then
    error("only available for response parser")
  end

  return self.llhttp:message_needs_eof()
end

---@return boolean should_keep_alive
function Parser:should_keep_alive()
  return self.llhttp:should_keep_alive()
end

---@return nil
function Parser:pause()
  return self.llhttp:pause()
end

---@return nil
function Parser:resume()
  return self.llhttp:resume()
end

---@return nil
function Parser:resume_after_upgrade()
  return self.llhttp:resume_after_upgrade()
end

---@return integer errno `llhttp_errno`
function Parser:get_errno()
  return self.llhttp:get_errno()
end

---@return string error_reason
function Parser:get_error_reason()
  return self.llhttp:get_error_reason()
end

---@param error_reason string
function Parser:set_error_reason(error_reason)
  return self.llhttp:set_error_reason(error_reason)
end

---@return string url
function Parser:pull_url()
  if not self._completed_url then
    error("url not parsed yet")
  end

  return self.llhttp:pull_url()
end

---@return table<string,string> headers
function Parser:pull_headers()
  if not self._completed_headers then
    error("headers not parsed yet")
  end

  return self.llhttp:pull_headers()
end

---@return string body_chunk
function Parser:pull_body_chunk()
  return self.llhttp:pull_body_chunk()
end

return Parser
