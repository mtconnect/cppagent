require 'net/http'
require './long_pull'
require 'socket'
require 'rexml/document'
require 'time'

if ARGV.length < 1
  puts "usage: seq_test <uri>"
  exit 9
end

$out = File.open("seq_test_#{Time.now.strftime('%Y%m%dT%H%M%S')}.log", 'w')
$out.sync = true

$last = Time.now
$count = 0
def dump(last, xml)
  nxt = nil
  document = REXML::Document.new(xml)
  document.each_element('//Header') { |x| 
    nxt = x.attributes['nextSequence'].to_i 
  }
  events = []
  document.each_element('//Events/*|//Samples/*|//Condition/*') do |event|
    events << event.attributes['sequence'].to_i
  end
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
  if (ts - $last).to_i > 30
    $out.puts "#{ts}: Received #{$count} at #{$count / (ts - $last)} events/second"
    $last = ts
    $count = 0
  end
  puts "#{ts}: *************** Next sequece not correct: last: #{last} next: #{nxt}" if last != nxt
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
nxt, $instance = 0, 0

puts "polling..."
begin
  begin
    puts "Connecting"
    client = Net::HTTP.new(dest.host, dest.port)
    newNxt, newInstance = current(client, rootPath)
    puts "Instance Id: #{newInstance} Next: #{newNxt}"
    if $instance != newInstance
      puts "New next: #{newNxt}"
      nxt = newNxt
      $instance = newInstance
    else
      puts "Continuing at: #{nxt}"
    end
  rescue
    puts $!.class
    puts $!
    client = nil
    sleep 5
  end while client.nil?
  if client
    path = rootPath + "sample?interval=1000&count=1000&from=#{nxt}"
    puller = LongPull.new(client)
    puller.long_pull(path) do |xml|
      nxt = dump(nxt, xml)
    end
  end
rescue
  puts $!
  puts $!.backtrace.join("\n")
  sleep 5
end while true
