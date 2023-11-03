require 'time'
require 'socket'
require 'optparse'
require 'thread'

loop_file = false
port = 7878
scenario = false
$verbose = false
fast = nil
host = '0.0.0.0'
max = nil
$heartbeat = 10000
$chaos = false

OptionParser.new do |opts|
  opts.banner = 'Usage: run_scenrio.rb [-lfh] [-s server] [-p port] <file>'

  opts.on('-l', '--[no-]loop', 'Loop file') do  |v|
    loop_file = v
  end

  opts.on('-p', '--port [port]', OptionParser::DecimalInteger, 'Port (default: 7878)') do  |v|
    port = v
  end

  opts.on('-h', '--help', 'Show help') do
    puts opts
    exit 1
  end

  opts.on('--heartbeat [interval]', OptionParser::DecimalInteger, "Heartbeat Frequency") do |v|
    $heartbeat = v.to_i
  end

  opts.on('-t', '--[no-]scenario', 'Run scenario or log') do |v|
    scenario = v
  end
  
  opts.on('-v', '--[no-]verbose', 'Verbose output') do |v|
    $verbose = v
  end
  
  opts.on('-f [rate]', OptionParser::DecimalNumeric, 'Pump as fast as possible') do |v|
    puts "FAST - Will deliver data at with #{v} seconds between items"
    fast = v.to_f
  end
  
  opts.on('-m [rate]', OptionParser::DecimalNumeric, 'Maximum amount of time to sleep between observations') do |v|
    max = v.to_f
  end
  
  opts.on('-s', '--server [server]', String, 'Server IP port to bind to (default: 0.0.0.0)') do |v|
    host = v
  end

  opts.on('-c', '--chaos', 'Random behavior of adapter') do |v|
    puts "Adding chaos to the stream to cause random disconnects"
    $chaos = v
  end
                                                              
  
  opts.parse!
  if ARGV.length < 1
    puts "Missing log file <file>"
    puts opts
    exit 1
  end
end

def heartbeat(socket)
  Thread.new do
    begin
      while (select([socket], nil, nil))
        if (r = socket.read_nonblock(256)) =~ /\* PING/
          if $chaos
            sleep rand * $heartbeat * 3.0
          end

          puts "Received #{r.strip}, responding with pong" if $verbose
          $mutex.synchronize {
            socket.puts "* PONG #{$heartbeat}"
            socket.flush
          }
        else
          puts "Received '#{r.strip}'" if $verbose
        end
      end
    rescue
      puts "Heartbeat thread error, exiting: #{$!}"
    end
  end
end

server = TCPServer.new(host, port)
sockets = []
$mutex = Mutex.new

Thread.new do
  while true
    puts "Waiting on #{server} #{port}"
    s = server.accept
    $mutex.synchronize {
      sockets << s
    }
    heartbeat(s)
    puts "Client connected"
  end
end


def format_time
  time = Time.now.utc
  time.strftime("%Y-%m-%dT%H:%M:%S.") + ("%06d" % time.usec) + "Z"
end

puts "Waiting for first connection"
while sockets.empty?
  sleep 1
end
sleep 1

begin
  File.open(ARGV[0]) do |file|
    last = nil
    file.each do |l|
      if scenario or l =~ /^\d{4,4}-\d{2,2}-\d{2,2}.+\|/o
        f, r = l.chomp.split('|', 2)
      
        if fast
          sleep fast
        elsif scenario
          if f == nil
            f = 1
          end
        else
          time = Time.parse(f)
      
          if last
            delta = time - last
            if delta > 0.0
              if max and delta > max
                sleep max
              else
                sleep delta
              end
            end
          end
        end
        line = "#{format_time}|#{r}"
      else
        line = l
      end
      
      if $chaos
        sleep 6.0 if (rand * 10.0) < 0.5
      end

      $mutex.synchronize {
        sockets.each do |socket|
          begin
            puts  line if $verbose        
            socket.puts line
            socket.flush
          rescue
            puts $!
            sockets.delete socket
          end
        end
      }
      last = time
      sleep(f.to_f) if scenario
    end
  end
end while (loop_file)
