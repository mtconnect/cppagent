require 'json'
require 'optparse'

zip = false

OptionParser.new do |opts|
  opts.banner = 'Usage: ruby get_package.rb [-z]'

  opts.on('-z', '--[no-]zip', 'Get the cpack zip package if available') do |v|
    zip = true
  end

  opts.parse!
end

json = `conan list -f json mtconnect_agent/*:*`
desc = JSON.parse(json)

name, = desc.first[1].first
package, = desc.first[1].first[1].first[1].first[1]['packages'].keys

package_dir = `conan cache path #{name}:#{package}`.strip

if (zip)
  file, = Dir["#{package_dir}/*"].select { |f| f =~ %r{/agent[^/]+\.zip$} }
  print file
else
  print package_dir
end
