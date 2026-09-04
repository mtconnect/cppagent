require 'strscan'

Dir['*_test.cpp'].each do |t|
  p t
  cpp = File.read(t)
  scanner = StringScanner.new(cpp)
  while m = scanner.scan_until(/TEST\(([A-Za-z]+),[ ]*([A-Za-z_]+)\)/) do
    p scanner.pre_match
    p scanner.matched
    p scanner.pos
  end
  exit
end
