task :default => 'multipart'

file 'multipart' => ['multipart.cpp', 'MultipartParser.h', 'MultipartReader.h'] do
	sh 'g++ -Wall -g multipart.cpp -o multipart'
end

task :generate_test_file do
	output   = ENV['OUTPUT']
	size     = (ENV['SIZE'] || 1024 * 1024 * 100).to_i
	boundary = ENV['BOUNDARY'] || 'abcd'
	raise 'OUTPUT must be specified' if !output
	
	File.open(ENV['OUTPUT'], 'wb') do |f|
		f.write("--#{boundary}\r\n")
		f.write("content-type: text/plain\r\n")
		f.write("content-disposition: form-data; name=\"field1\"; filename=\"field1\"\r\n")
		f.write("foo-bar: abc\r\n")
		f.write("x: y\r\n")
		f.write("\r\n")
		
		File.open("/dev/urandom", 'rb') do |r|
			written = 0
			buf = ''
			while !r.eof? && written < size
				r.read([1024, size - written].min, buf)
				f.write(buf)
				written += buf.size
			end
		end
		
		f.write("\r\n--#{boundary}--\r\n")
	end
end