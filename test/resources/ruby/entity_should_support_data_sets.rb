begin
  puts "Creating a simple entity"
  
  $ent1 = MTConnect::Entity.new("TestEntity", { VALUE: { string: 'value1', int: 100, float: 123.4 } })
  puts "Created an entity"
  
  raise "Entity is null" unless $ent1

  puts "Getting value"
  value = $ent1.value
  raise "No value" unless value

  raise "Bad string value" unless value[:string] == 'value1'
  raise "Bad int value" unless value[:int] == 100
  raise "Bad float value" unless value[:float] == 123.4

rescue
  puts "Caught exception: #{$!}"
  puts $!.bactrace
end



