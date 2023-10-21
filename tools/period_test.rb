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
  document.each_element('//Samples/Angle') do |event|
    events << [event.attributes['timestamp'], event.text]
  end
  return [nxt, events]
end

def dump(last, xml)
  nxt, events = parse(xml)

  p events
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
  rescue
    puts $!.class
    puts $!
    client = nil
    sleep 5
  end while client.nil?
  if client
    path = rootPath + "sample?interval=500&count=10000"
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
