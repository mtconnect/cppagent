MTConnect::Logger.info "Declaring class FixExecution"

$trans =  MTConnect::RubyTransform.new("FixTransform", :Event) { |obs|
  puts "#{obs.name}: #{obs.value}"
  p obs
  if obs.data_item.type == 'EXECUTION'
    puts "Got an execution event"
    obs = obs.dup
    case obs.value
    when "1"
      obs.value = "READY"
      
    when "2"
      obs.value = "ACTIVE"
    end
  end
  
  forward(obs)  
}
