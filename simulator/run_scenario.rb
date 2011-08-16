require 'time'
require 'socket'
require 'optparse'
require 'thread'

loop_file = false
port = 7878
scenario = false
verbose = false
fast = false

OptionParser.new do |opts|
  opts.banner = 'Usage: run_scenrio.rb [-l] <file>'

  opts.on('-l', '--[no-]loop', 'Loop file') do  |v|
    loop_file = v
  end

  opts.on('-p', '--port [port]', OptionParser::DecimalNumeric, 'Port (default: 7878)') do  |v|
    port = v
  end

  opts.on('-h', '--help', 'Show help') do
    puts opts
    exit 1
  end

  opts.on('-s', '--[no-]scenario', 'Run scenario or log') do |v|
    scenario = v
  end
  
  opts.on('-v', '--[no-]verbose', 'Verbose output') do |v|
    verbose = v
  end
  
  opts.on('-f', '--[no-]fast', 'Pump as fast as possible') do |v|
    fast = v
  end

  opts.parse!
  if ARGV.length < 1
    puts "Missing log file <file>"
    puts opts
    exit 1
  end
end

puts "Waiting on 0.0.0.0 #{port}"
server = TCPServer.new(port)
socket = server.accept
puts "Client connected"

mutex = Mutex.new

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


def format_time
  time = Time.now.utc
  time.strftime("%Y-%m-%dT%H:%M:%S.") + ("%06d" % time.usec)
end

if scenario
  begin
    File.open(ARGV[0]) do |file|
      file.each do |l|
        r, f = l.chomp.split('|', 2)
        r = 1 unless r
        
        line = "#{format_time}|#{f}"
        mutex.synchronize {
          socket.puts line
          socket.flush
        }
        puts line if verbose
        sleep(r.to_f)
      end
    end
  end while (loop_file)
else
  begin
    File.open(ARGV[0]) do |file|
      last = nil
      file.each do |l|
        next unless l =~ /^\d{4,4}-\d{2,2}-\d{2,2}.+\|/o
        f, r = l.chomp.split('|', 2)
        time = Time.parse(f)
        
        # Recreate the delta time
        unless fast
          if last
            delta = time - last
            sleep delta if delta > 0.0
          end
        else
          sleep 0.01
        end
        
        line = "#{format_time}|#{r}"
        mutex.synchronize {
          socket.puts line
          socket.flush
        }
        puts  line if verbose        
        last = time
      end
    end
  end while (loop_file)
end
