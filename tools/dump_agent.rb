require 'net/http'
require './long_pull.rb'
require 'socket'
require 'rexml/document'

if ARGV.length < 2
  puts "usage: dump_agent <uri> <output>"
  exit 9
end

def dump(out, xml)
  nxt = nil
  document = REXML::Document.new(xml)
  document.each_element('//Header') { |x| 
    nxt = x.attributes['nextSequence'].to_i 
  }
  events = []
  document.each_element('//Events/*|//Samples/*|//Condition/*') do |event|
    events << event
  end
  events = events.sort_by { |n| n.attributes['sequence'].to_i }
  
  events.each do |event|
    name = event.attributes['name'] 
    name = event.attributes['dataItemId'] if name.nil? or name.empty?
    out.print "#{event.attributes['timestamp']}|#{name}|"
    case event.name
    when 'Alarm'
      out.print "#{event.attributes['code']}|#{event.attributes['nativeCode']}|#{event.attributes['severity']}|#{event.attributes['state']}|"
      
    when 'Normal', 'Warning', 'Fault', 'Unavailable'
      out.print "#{event.name}|#{event.attributes['nativeCode']}|#{event.attributes['nativeSeverity']}|#{event.attributes['qualifier']}|"
      
    when 'Message'
      out.print "#{event.attributes['nativeCode']}|"
      
    end
    out.puts event.text.to_s
    puts "#{event.name} - #{name} - #{event.text}"
  end
  print '.'; STDOUT.flush
  nxt
end

def current(out, client, path)
  current = path.dup + 'current'
  resp = client.get(current)
  document = REXML::Document.new(resp.body)
  nxt, instance = nil
  document.each_element('//Header') { |x| 
    nxt = x.attributes['nextSequence'].to_i 
    instance = x.attributes['instanceId'].to_i
  }
  dump(out, resp.body)
  [nxt, instance] 
end

File.open(ARGV[1], 'w') do |out|
  dest = URI.parse(ARGV[0])
  client = Net::HTTP.new(dest.host, dest.port)
  client.use_ssl = dest.scheme == 'https'
  
  path = dest.path
  path += '/' unless path[-1] == ?/
  rootPath = path.dup

  nxt, instance = current(out, client, rootPath)
  puts "Instance Id: #{instance}"
  
  puts "polling..."
  begin
    path = rootPath + "sample?interval=1000&count=1000&from=#{nxt}"
    puller = LongPull.new(client)
    puller.long_pull(path) do |xml|
      nxt = dump(out, xml)
    end
    puts "Reconnecting"
    client = Net::HTTP.new(dest.host, dest.port)
    newNxt, newInstance = current(STDOUT, client, rootPath)
    if instance != newInstance
      nxt = newNxt
      instance = newInstance
    end
  rescue
  end while true
end
