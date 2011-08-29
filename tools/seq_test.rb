require 'net/http'
require './long_pull'
require 'socket'
require 'rexml/document'

if ARGV.length < 1
  puts "usage: seq_test <uri>"
  exit 9
end

$last = nil
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
  puts "First: #{events.first}"
  events.each do |n|
    if last != n
      puts "*************** Missed event #{last}"
      last = n
    end
    last += 1
  end
  puts "Last: #{events.last}"
  ts = Time.now
  puts "Received #{events.size} at #{events.size / (ts - $last)} msgs/second" if $last
  $last = ts
  puts "*************** Next sequece not correct: last: #{last} next: #{nxt}" if last != nxt
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
client = Net::HTTP.new(dest.host, dest.port)

path = dest.path
path += '/' unless path[-1] == ?/
rootPath = path.dup

nxt, instance = current(client, rootPath)
puts "Instance Id: #{instance} Next: #{nxt}"

puts "polling..."
begin
  path = rootPath + "sample?interval=1000&count=1000&from=#{nxt}"
  puller = LongPull.new(client)
  puller.long_pull(path) do |xml|
    nxt = dump(nxt, xml)
  end
  puts "Reconnecting"
  client = Net::HTTP.new(dest.host, dest.port)
  newNxt, newInstance = current(client, rootPath)
  if instance != newInstance
    nxt = newNxt
    instance = newInstance
  end
rescue
  puts $!
  puts $!.backtrace.join("\n")
  exit 1
end while true
