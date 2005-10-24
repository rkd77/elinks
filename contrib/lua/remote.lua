-- ELinks-side part of elinks-remote

-- See script elinks-remote for explanation what's this about.

----------------------------------------------------------------------
--  User options
----------------------------------------------------------------------

-- File to look in for external url to jump to
   external_url_file = elinks_home.."/external.url"


----------------------------------------------------------------------
--  Implementation
----------------------------------------------------------------------

function external_url ()
   fh = io.open (external_url_file, "r")
   aline = current_url ()
   if fh then
      aline = read (fh, "*l")
      io.close (fh)
   else
      print ("Couldn't open outfile")
   end
   return aline
end

   bind_key ("main", "x",
             function () return "goto_url", external_url () end)


function set_external_url ()
   fh = io.open (external_url_file, "w")
   aline = current_link ()
   if fh then
      write (fh, aline.."\n")
      io.close (fh)
   else
      print ("Couldn't open outfile")
   end
end

    bind_key ("main", "X",
              function () set_external_url () end)

-- vim: shiftwidth=4 softtabstop=4
