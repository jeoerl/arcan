class Mame
	def initialize
		@categories = {}
		@series = {}
		@mameargs = [ [], [], [] ] 
		@target = Target.new
	end

	def merge_gameinfo(catver)
		STDERR.print("[MAME importer] processing catver file\n")

		File.open(catver, File::RDONLY).each_line{|line|
			break if line[0..9] == "[VerAdded]"
			name, category = line.split(/\=/)
			maincat, subcat = category.split(/\//) if category
			maincat.strip! if maincat
			subcat.strip! if subcat
			@categories[name] = [maincat, subcat]
		}	
		STDERR.print("[MAME importer::catver], categories loaded\n")	
		true

	rescue => er
		STDERR.print("[MAME importer::catver] Couldn't open category file, #{catver} -- #{er}\n")
		false
	end 

	def merge_relatives(series)
		STDERR.print("[MAME importer::series] processing series file\n")
		infile = File.open(series, File::RDONLY).each_line{|line|
			cline = line.strip
			next if cline.size == 0
			
			if cline =~ /\[(\w|\s)*\]/
				gname = cline[1..-2]
			else
				@series[cline] = gname
			end
		}
		STDERR.print("[MAME importer::series] series processed\n")
		true

	rescue => er
		STDERR.print("Couldn't open series file, #{series}\n")
		false
	end

	def set_defaults(options, cmdopts)
		@mameargs[0] << "-rompath"
		@mameargs[0] << "[gamepath]/mame"
		@mameargs[0] << "-cfg_directory"
		@mameargs[0] << "[themepath]/_mame/cfg"
		@mameargs[0] << "-nvram_directory"
		@mameargs[0] << "[themepath]/_mame/nvram"
		@mameargs[0] << "-memcard_directory"
		@mameargs[0] << "[themepath]/_mame/memcard"
		@mameargs[0] << "-input_directory"
		@mameargs[0] << "[themepath]/_mame/input"
		@mameargs[0] << "-state_directory"
		@mameargs[0] << "[themepath]/_mame/state"
		@mameargs[0] << "-snapshot_directory"
		@mameargs[0] << "[themepath]/_mame/snapshot"
		@mameargs[0] << "-diff_directory"
		@mameargs[0] << "[themepath]/_mame/diff"
		@mameargs[0] << "-comment_directory"
		@mameargs[0] << "[themepath]/_mame/comment"
		@mameargs[0] << "-skip_gameinfo"

# internal launch arguments, we want the data as "pure" as and "cheap" possible
		@mameargs[1] << "-window"
		@mameargs[1] << "-scalemode"
		@mameargs[1] << "none" 
		@mameargs[1] << "-nomaximize"
		@mameargs[1] << "-multithreading"
		@mameargs[1] << "-keepaspect"
		
		if (res = cmdopts["--mamecatver"])
			merge_gameinfo( res[0] )
		end

		if (res = cmdopts["--mameseries"])
			merge_relatives( res[0] )
		end

		@onlygood = cmdopts["--mamegood"] ? true : false
		@verify = cmdopts["--mameverify"] ? true : false
		@skipclone = cmdopts["--mameskipclone"] ? true : false
		@shorttitle = cmdopts["--mameshorttitle"] ? true : false

		chkargs = ["--mameargs", "--mameintargs", "--mameextargs"]
		chkargs.each_with_index{|argstr, ind|
			if (arg = cmdopts[argstr])
				arg.each{|subarg|
					subargs = subarg.split(/,/)
					if (subargs.size < 1)
						STDERR.print("[MAME Importer] couldn't set #{argstr}, format; arg1,arg2,...")
						return false
					else
						@mametargets[ind] = arg[1..-1]
					end
				}
			end
		}
	end

	def accepted_arguments
		[ 
			["--mamecatver", GetoptLong::REQUIRED_ARGUMENT],
			["--mameseries", GetoptLong::REQUIRED_ARGUMENT],
			["--mameverify", GetoptLong::NO_ARGUMENT ],
			["--mameargs", GetoptLong::REQUIRED_ARGUMENT],
			["--mameintargs", GetoptLong::REQUIRED_ARGUMENT],
			["--mameextargs", GetoptLong::REQUIRED_ARGUMENT],
			["--mamegood", GetoptLong::NO_ARGUMENT ],
			["--mameskipclone", GetoptLong::NO_ARGUMENT],
			["--mameshorttitle", GetoptLong::NO_ARGUMENT]
		]
	end

	def usage()
	   [
		"(--mamecatver) filename - Specify a catver.ini file",
		"(--mameseries) filename - Specify a series.ini file",
        "(--mameverify) - Only add games that pass verification",
		"(--mameargs) - comma-separated list of launch arguments",
		"(--mameintargs) - comma- separated list of internal- launch arguments",
		"(--mameextargs) - comma- separated list of external- launch arguments",
		"(--mamegood) - Only add games where the driver emulation status is good",
		"(--mameskipclone) - Skip drivers that are marked as clones",
		"(--mameshorttitle) - Don't store extraneous title data (set,revision,..)"
		]
	end

	def checkrom(mamepath, rompath, romset)
		a = IO.popen("#{mamepath} -rompath #{rompath} -verifyroms #{romset} 1> /dev/null 2>/dev/null")
		a.readlines
		a.close

	return $?.exitstatus == 0
	
	rescue => ex
		false
	end

# wrap new (0.144ish?) "joy -> 4,8, ..." ways into the older version, since that one was heavilly embedded already
	def subjoy_str(waylbl, waylbl2)
	    resstr = waylbl2 ? "doublejoy" : "joy"
	    case waylbl
	    when "2"
	        resstr << "2way"
	    when "4"
	        resstr << "4way"
	    when "8"
	        resstr << "8way"
	    when "vertical2"
	        resstr = "v#{resstr}2way"
	    else
	        resstr = nil # silently ignore
	    end
	    resstr
	end

	def convert_inputtype(inputnode)
	    res = nil
	    case inputnode.attributes["value"]
	    when "gambling"
	    when "hanafuda"
	    when "mahjong"
	    when "joy"
	        res = subjoy_str(inputnode.attributes["ways"], nil)
	    when "doublejoy"
	        res = subjoy_str(inputnode.attributes["ways"], inputnode.attributes["ways2"])
	    else
	        res = inputnode.attributes["value"]
	    end
	    res 
	end

# use the internal gamedb of mame from the verified target,
# splice subtrees off as separate documents and parse individually,
# mem-usage is insane otherwise.
	def mame_each
		status = {}
		xmlin = IO.popen("#{@mametarget} -listxml")

		endid = 0
		
# seriously ugly hack around Nokogiri versioning issues
		begin
			endid = Nokogiri::XML::Reader::TYPE_END_ELEMENT
		rescue
			endid = 15
		end
		
		xp = Nokogiri::XML::Reader.from_io( xmlin )
		while (xp.read)
			next unless xp.name == "game"
			next if xp.node_type == endid
			
			node_tree = Nokogiri::XML.parse( xp.outer_xml.dup ) 
			next if node_tree.root.attributes["isbios"]
			next if @skipclone and node_tree.root.attributes["cloneof"]

			title = node_tree.xpath("//game/description").text
			shorttitle = title.split(/\s\(/)[0]

			res = Game.new
			res.target = @target
			res.title = @shorttitle ? shorttitle : title 
			res.ctrlmask = 0
			res.year  = node_tree.xpath("//game/year").text
			res.manufacturer = node_tree.xpath("//game/manufacturer").text
			res.setname = node_tree.root.attributes['name'].value
			if (@categories[res.setname])
				res.genre = @categories[res.setname][0]
				res.subgenre = @categories[res.setname][1]
			end
			
			driver = node_tree.xpath("//game/driver")[0]
			
			next if (driver == nil)
			next if (@onlygood and driver.attributes["status"].value != "good")

			node_input = node_tree.xpath("//game/input")[0]
			res.players = node_input.attributes["players"].value
			res.buttons = node_input.attributes["buttons"]
			res.buttons = res.buttons == nil ? 0 : res.buttons.value
			res.target  = @target

# note, there can be some "weird" combinations, i.e. paddle + pedal, stick + pedal, stick + padel + pedal
# and the format as such changes heavilly between version, horray. 
			node_input.children.each{|child|
				if (child and child.attributes["type"])
#				    inputlabel = convert_inputtype(child)
#				    res.ctrlmask = res.ctrlmask | res.get_mask( mmask ) if inputlabel
				end 
			}

			yield res
		end
	end

	def check_target(target, targetpath)
		executable = ""

		if File.exists?("#{targetpath}/mame")
			executable = "mame"
		elsif File.exists?("#{targetpath}/mame.exe")
			executable = "mame.exe"
		else
			raise "Could not locate mame (tried #{targetpath}/mame and #{targetpath}/mame.exe, giving up."
		end

		@mametarget = "#{targetpath}/#{executable}"
		@target = Target.Load(0, "mame")

		if (@target == nil)
			@target = Target.Create("mame", executable, @mameargs)
		else
			@target.arguments = @mameargs
			@target.store
		end

		return (File.exists?(@mametarget) and File.stat(@mametarget).executable?)
	end

	def check_games(rompath)
		romset = {}

		dn = Dir["#{rompath}/*.zip"].each{|romname|
			basename = romname[ romname.rindex('/')+1 .. romname.rindex('.')-1 ]
			romset[basename] = true
		}

		mame_each{|game| 
			if romset[game.setname] 
				if (@verify and checkrom(@mametarget, rompath, game.setname) == false)
					STDERR.print("[MAME Importer] romset (#{game.setname}) failed verification.\n")
				else 
					yield game
				end
			end
		}

		true

	rescue => er
		STDERR.print("[MAME importer] failed, reason: #{er}, #{er.backtrace}\n")
		false
	end

	def to_s
		"[MAME Importer]"
	end
end