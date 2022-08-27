rockspec_format = "3.0"
package = "llhttp"
version = "0.0.0"
source = {
  url = "git+https://github.com/MunifTanjim/llhttp.lua.git",
  tag = "0.0.0",
}
description = {
  summary = "Lua interface for llhttp.",
  detailed = [[
    Lua interface for llhttp.
  ]],
  license = "MIT",
  homepage = "https://github.com/MunifTanjim/llhttp.lua",
  issues_url = "https://github.com/MunifTanjim/llhttp.lua/issues",
  maintainer = "Munif Tanjim (https://muniftanjim.dev)",
  labels = { "http", "llhttp" },
}
build = {
  type = "builtin",
  modules = {
    ["llhttp"] = "llhttp/init.lua",
  },
}
