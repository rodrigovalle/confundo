confundo = Proto("confundo", "CS118 Confundo Transport Protocol (CTP)")

local f_seqno  = ProtoField.uint32("confundo.seqno",        "Sequence Number")
local f_ack    = ProtoField.uint32("confundo.ack",          "ACK Number")
local f_id     = ProtoField.uint16("confundo.connectionId", "Connection ID")
local f_flags  = ProtoField.uint16("confundo.flags",        "Flags")

confundo.fields = { f_seqno, f_ack, f_id, f_flags }

function confundo.dissector(tvb, pInfo, root) -- Tvb, Pinfo, TreeItem
   if (tvb:len() ~= tvb:reported_len()) then
      return 0
   end

   local t = root:add(confundo, tvb(0,12))
   t:add(f_seqno, tvb(0,4))
   t:add(f_ack, tvb(4,4))
   t:add(f_id, tvb(8,2))
   local f = t:add(f_flags, tvb(10,2))

   local flag = tvb(11,1):uint()

   if bit.band(flag, 1) ~= 0 then
      f:add(tvb(11,1), "FIN")
   end
   if bit.band(flag, 2) ~= 0 then
      f:add(tvb(11,1), "SYN")
   end
   if bit.band(flag, 4) ~= 0 then
      f:add(tvb(11,1), "ACK")
   end
  
   pInfo.cols.protocol = "Confundo"
end

local udpDissectorTable = DissectorTable.get("udp.port")
udpDissectorTable:add("5000", confundo)

io.stderr:write("confundo.lua is successfully loaded\n")
