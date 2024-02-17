require 'net/http'
require './long_pull'
require 'socket'
require 'rexml/document'
require 'time'

require 'optparse'
$fast = false
OptionParser.new do |opts|
  opts.banner = 'Usage: seq_test.rb [-f] <url>'
  
  opts.on('-f', '--[no-]fast', 'Fast mode, do not parse XML [Default: false]') do  |v|
    $fast = v
  end
  
  opts.parse!
  if ARGV.length < 1
    puts "Missing url <url>"
    puts opts
    exit 1
  end
end


$out = File.open("seq_test_#{Time.now.strftime('%Y%m%dT%H%M%S')}.log", 'w')
$out.sync = true

$last_log_time = Time.now
$count = 0
$total_bytes = 0

def parse_fast(xml)
  if xml !~ /MTConnectStreams/o
    $out.puts xml
    return [0, []]
  end
  m = /nextSequence="(\d+)"/o.match(xml)
  if m
    nxt = m[1].to_i
  else
    puts "Could not find next sequence in xml"
    puts xml
    return [0, []]
  end
  events = xml.scan(/sequence="(\d+)"/o).flatten.map { |n| n.to_i }
  [nxt, events]
end

def parse(xml)
  document = REXML::Document.new(xml)
  if document.root.name == 'MTConnectError'
    $out.puts xml
    return [0, []]
  end
  nxt = nil
  document.each_element('//Header') { |x| 
    nxt = x.attributes['nextSequence'].to_i 
  }
  unless nxt
    puts "Could not find next sequence in xml"
    puts xml
    return [0, []]
  end
  events = []
  document.each_element('//Events/*|//Samples/*|//Condition/*') do |event|
    events << event.attributes['sequence'].to_i
  end
  return [nxt, events]
end

def dump(last, xml)
  if $fast
    nxt, events = parse_fast(xml)
  else
    nxt, events = parse(xml)
  end
  if nxt != 0
    events.sort!
    events.each do |n|
      if last != n
        $out.puts "#{Time.now}: *************** Missed event #{last}"
        last = n
      end
      last += 1
    end
    ts = Time.now
    $count += events.size
    $total_bytes += xml.length if events.size > 0
    
    if (ts - $last_log_time).to_i > 10
      $out.puts "#{ts}: Received #{$count} at #{$count / (ts - $last_log_time)} events/second"
      $out.puts "       Average event size is #{$total_bytes / $count} bytes" if $count > 0

      puts "#{ts}: Received #{$count} at #{$count / (ts - $last_log_time)} events/second"
      puts "       Average event size is #{$total_bytes / $count} bytes" if $count > 0
      
      $last_log_time = ts
      $count = 0
      $total_bytes = 0
    end
    puts "#{ts}: *************** Next sequece not correct: last: #{last} next: #{nxt}" if last != nxt
  end
  nxt
end

def current(client, path)
  current = path.dup + 'current'
  resp = client.get(current)
  document = REXML::Document.new(resp.body)
  nxt, instance = nil
  document.each_element('//Header') { |x| 
    nxt = x.attributes['nextSequence'].to_i 
    instance = x.attributes['instanceId'].to_i
  }
  [nxt, instance] 
end

dest = URI.parse(ARGV[0])
path = dest.path
path += '/' unless path[-1] == ?/
rootPath = path.dup
client = nil
$nxt, $instance = 0, 0

puts "polling..."
begin
  begin
    puts "Connecting"
    client = Net::HTTP.new(dest.host, dest.port)
    newNxt, newInstance = current(client, rootPath)
    puts "Instance Id: #{newInstance} Next: #{newNxt}"
    if $instance != newInstance or $nxt == 0
      puts "New next: #{newNxt}"
      $nxt = newNxt
      $instance = newInstance
    else
      puts "Continuing at: #{$nxt}"
    end
  rescue
    puts $!.class
    puts $!
    client = nil
    sleep 5
  end while client.nil?
  if client
    path = rootPath + "sample?interval=100&count=10000&from=#{$nxt}"
    puller = LongPull.new(client)
    puller.long_pull(path) do |xml|
      $nxt = dump($nxt, xml)
    end
  end
rescue
  puts $!
  puts $!.backtrace.join("\n")
  sleep 5
end while true
