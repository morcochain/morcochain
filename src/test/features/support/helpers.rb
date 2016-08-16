require 'timeout'
def wait_for(timeout_ratio = 1.0)
  timeout = (ENV["TIMEOUT"] || 5.0).to_f * timeout_ratio
  last_exception = nil
  begin
    Timeout.timeout(timeout) do
      loop do
        begin
          break if yield
        rescue RSpec::Expectations::ExpectationNotMetError, RuntimeError => e
          last_exception = e
        end
        sleep 0.1
      end
    end
  rescue Timeout::Error
    if last_exception
      raise last_exception
    else
      raise
    end
  end
end

def unit(unit_name)
  case unit_name.strip
  when "NuShare", 'NuShares', 'NSR', "" then 'S'
  when 'NuBit', 'NuBits', 'NBT' then 'B'
  else raise "Unknown unit: #{unit_name.inspect}"
  end
end

def money_supply(unit_name, node = @nodes.values.first)
  node.unit_rpc(unit(unit_name), "getinfo")["moneysupply"]
end

def parse_number(n)
  n.gsub(',', '').to_f
end


