
require 'socket'

if ARGV.length < 3
  puts "usage: dump_adapter <host> <port> <output>"
  exit 9
end

sock = TCPSocket.new(ARGV[0], ARGV[1].to_i)

File.open(ARGV[2], 'w') do |out|
  while (buffer = sock.read(1024))
    out.write(buffer)
  end
end

