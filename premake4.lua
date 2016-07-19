-- Define the install prefix
prefix = nil
    -- Installation under Linux
    if (os.is('linux')) then
        prefix = '/usr/local'
        os.execute('sudo chown -R `whoami` ' .. prefix .. ' && sudo chmod -R 751 ' .. prefix)

    -- Installation under Mac OS X
    elseif (os.is('macosx')) then
        prefix = '/usr/local'

    -- Other platforms
    else
        print(string.char(27) .. '[31mThe installation is not supported on your platform' .. string.char(27) .. '[0m')
        os.exit()
    end

solution 'chameleon'
    configurations {'Release', 'Debug'}
    location 'build'

    newaction {
        trigger = "install",
        description = "Install the library",
        execute = function ()
            os.execute('rm -rf ' .. path.join(prefix, 'include/chameleon'))
            os.mkdir(path.join(prefix, 'include/chameleon'))
            for index, filename in pairs(os.matchfiles('source/*.hpp')) do
                os.copyfile(filename, path.join(prefix, 'include/chameleon', path.getname(filename)))
            end

            print(string.char(27) .. '[32mChameleon library installed.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    newaction {
        trigger = 'uninstall',
        description = 'Remove all the files installed during build processes',
        execute = function ()
            os.execute('rm -rf ' .. path.join(prefix, 'include/chameleon'))
            print(string.char(27) .. '[32mChameleon library uninstalled.' .. string.char(27) .. '[0m')
            os.exit()
        end
    }

    project 'chameleonTest'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
