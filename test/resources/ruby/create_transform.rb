require 'time'

MTConnect::Logger.info "Declaring class FixExecution"
class FixExecution < MTConnect::RubyTransform
  def initialize
    super("FixExecution", :Event)
  end       
  
  def transform(obs)
    puts "#{obs.name}: #{obs.value}"
    if obs.data_item.type == 'EXECUTION'
      puts "Got an execution event"
      obs = obs.copy
      case obs.value
      when "1"
        obs.value = "READY"
        
      when "2"
        obs.value = "ACTIVE"
      end
    end
    
    forward(obs)
  end
end
