require 'wimax'
require 'posix'

w = wimax.open (wimax.PRIVILEGE_READ_ONLY)
dl = w:get_device_list ()

print ("wimax device list")
dev_index = 0
for k, v in pairs (dl) do
  print (k, v)
  dev_index = k
end

print ("opening device at index " .. dev_index)
w:device_open (dev_index)

print ("begin monitoring link status at 5 second intervals")
while true do
  local ls = w:get_link_status ()
  if nil == ls then
    print ("unable to get link status... exiting")
    break
  end

  print ("Freq: " .. ls.freq .. "KHz" .. " | "
      .. "RSSI: " .. ls.rssi .. "dBm" .. " | "
      .. "CINR: " .. ls.cinr .. "dB" .. " | "
      .. "TXPWR:" .. ls.txpwr .. "dBm")

  posix.sleep (5)
end

w:device_close ()
w:close ()
