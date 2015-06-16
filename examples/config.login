root = "./"
thread = 1
logger = nil
harbor = 0
start = "main"
bootstrap = "snlua bootstrap"	-- The service for bootstrap
luaservice = root.."../../../service/?.lua;"..root.."../../../examples/login/?.lua"
lualoader = root.."../../../lualib/loader.lua"
cpath = root.."cservice/?.dll"

lua_path = root.."../../../lualib/?.lua;"..root.."../../../examples/?.lua;"
lua_cpath = root.."luaclib/?.dll"
