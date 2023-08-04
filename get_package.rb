require 'json'

json = `conan list -f json mtconnect_agent/*:*`

desc = JSON.parse(json)

name, = desc.first[1].first
package, = desc.first[1].first[1].first[1].first[1]['packages'].keys
build_dir = `conan cache path #{name}:#{package}`.strip

file, = Dir["#{build_dir}/*"].select { |f| f =~ %r{/agent[^/]+\.zip$} }

print file
