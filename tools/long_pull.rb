# This is Will's code from the demo to parse the continuous response
# from the server.

require 'net/http'
require 'strscan'

class LongPull
  def initialize(http)
    @http = http
  end

  def long_pull(uri)
    @http.request_get(uri) do |res|
      content_type, = res.get_fields('content-type')
      puts "#{content_type}"
      if content_type !~ /boundary=([^;]+)/
        puts res.body
        raise "No boundry: #{res.body}"
      end
      
      boundary = "--#{$1}"
      puts "Boundary: #{boundary}"
      
      document = StringScanner.new(String.new)
      header = true
      length = boundary.length
      
      res.read_body do |chunk|
        next if chunk.empty?
        document << chunk

        while document.rest_size >= length
          if header
            if not document.check(/^#{boundary}/)
              puts document.rest
              raise "Framing error"
            end
                        
            break unless head = document.scan_until(/\r\n\r\n/)
            bound, *rest = head.split(/\r\n/)

            fields = Hash[*rest.map { |s| s.split(/:\s*/) }.flatten]
            length = fields['Content-Length'].to_i
            header = false
          else
            rest = document.rest
            
            # Slice off the chunk we need
            body = rest[0, length]
            
            document.clear
            document = StringScanner.new(rest[(length + 2)..-1])
            yield body
            # puts body
            
            # New message
            length = boundary.length
            header = true
          end
        end
      end
    end
  end
end
