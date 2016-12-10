
require 'socket'
require 'net/http'
require 'readline'
require 'time'


server = TCPServer.new('0.0.0.0', ARGV[0].to_i)

puts "Waiting for connection on #{ARGV[0].to_i}..."

loop do
  socket = server.accept
  Thread.new do
    while (select([socket], nil, nil))
      begin
        if (r = socket.read_nonblock(256)) =~ /\* PING/
          puts "Received #{r.strip}, responding with pong" if verbose
          mutex.synchronize {
            socket.puts "* PONG 10000"
          }
        else
          puts "Received '#{r.strip}'"
        end
      rescue
      end
    end
  end
  
  puts "Client connected"
  puts "Type an adapter feed string in the following format:"
  puts "> <key>|<value>"
  puts "> <key>|<value>|<key>|<value> ..."
  puts "> <alarm>|<code>|<native code>|<severity:CRITICAL|ERROR|WARNING|INFO>|<state:ACTIVE|CLEARED|INSTANT>|<message>"
  puts "> <condition>|<level:unavailable,normal,warning,fault>|<native code>|<native severity>|<qualifier>|<message>"
  loop do
    line = Readline::readline('> ')
    Readline::HISTORY.push(line)

    if line[0] == ?*
      puts "Writing line"
      socket.write "#{line}\n"
      socket.flush
    else
      ts = Time.now.utc
      stamp = "#{ts.iso8601[0..-2]}.#{'%06d' % ts.tv_usec}"
      puts "#{stamp}|#{line}"
      socket.write "#{stamp}|#{line}\n"
      socket.flush
    end
    
  end
end
