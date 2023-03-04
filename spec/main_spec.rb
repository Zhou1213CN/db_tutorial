describe 'database' do
    def run_script(commands)
      raw_output = nil
      IO.popen("./a.out", "r+") do |pipe|
        commands.each do |command|
          pipe.puts command
        end
  
        pipe.close_write
  
        # Read entire output
        raw_output = pipe.gets(nil)
      end
      raw_output.split("\n")
    end
  
    it 'inserts and retrieves a row' do
      result = run_script([
        "insert 1 user1 person1@example.com",
        "select",
        ".exit",
      ])
      expect(result).to match_array([
        "db > Executed.",
        "db > (1, user1, person1@example.com)",
        "Executed.",
        "db > ",
      ])
    end

    it 'prints error message when table is full' do
      script = (1..1401).map do |i|
        "insert #{i} user#{i} person#{i}@example.com"
      end
      script << ".exit"
      result = run_script(script)
      expect(result[-2]).to eq('db > Error: Table full.')
    end

    # Fails 
    # If you take a look at our definition of a Row, we allocate exactly 32 bytes for username and exactly 255 bytes for email. But C strings are supposed to end with a null character, which we didn’t allocate space for. The solution is to allocate one additional byte
    it 'allows inserting strings that are the maximum length' do
      long_username = "a"*32
      long_email = "a"*255
      script = [
        "insert 1 #{long_username} #{long_email}",
        "select",
        ".exit",
      ]
      result = run_script(script)
      expect(result).to match_array([
        "db > Executed.",
        "db > (1, #{long_username}, #{long_email})",
        "Executed.",
        "db > ",
      ])
    end

    it 'prints error message if strings are too long' do
      long_username = "a"*33
      long_email = "a"*256
      script = [
        "insert 1 #{long_username} #{long_email}",
        "select",
        ".exit",
      ]
      result = run_script(script)
      expect(result).to match_array([
        "db > String is too long.",
        "db > Executed.",
        "db > ",
      ])
    end

    it 'prints an error message if id is negative' do
      script = [
        "insert -1 cstack foo@bar.com",
        "select",
        ".exit",
      ]
      result = run_script(script)
      expect(result).to match_array([
        "db > ID must be positive.",
        "db > Executed.",
        "db > ",
      ])
    end

  end