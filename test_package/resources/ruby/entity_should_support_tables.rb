begin
  puts "Creating a simple entity"
  
  $ent1 = MTConnect::Entity.new("TestEntity",
                                  VALUE: {
                                    row1: { string: 'text1', float: 1.0 },
                                    row2: { string: 'text2', float: 2.0 }
                                  })
  puts "Created an entity"
  
  raise "Entity is null" unless $ent1

  puts "Getting value"
  value = $ent1.value
  raise "No value" unless value

  p value

rescue
  puts "Caught exception: #{$!}"
  puts $!.bactrace
end



