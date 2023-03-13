/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Core string lists
 *
 * three macros must be defined to use this header
 * CORESTRING_LWC_VALUE - wapcaplet strings with a value not derived from name
 * CORESTRING_DOM_VALUE - dom strings with a value not derived from name
 *
 * two helper macros are defined that allow simple mapping strings
 * CORESTRING_LWC_STRING - libwapcaplet strings with a simple name value mapping
 * CORESTRING_DOM_STRING - dom strings with a simple name value mapping
 *
 * \note This header is specificaly intented to be included multiple
 *   times with different macro definitions so there is no guard.
 */

#if !defined(CORESTRING_LWC_VALUE) | !defined(CORESTRING_DOM_VALUE) //| !defined(CORESTRING_NSURL)
#error "missing macro definition. This header must not be directly included"
#endif

#undef CORESTRING_LWC_STRING
#define CORESTRING_LWC_STRING(NAME) CORESTRING_LWC_VALUE(NAME, #NAME)

#undef CORESTRING_DOM_STRING
#define CORESTRING_DOM_STRING(NAME) CORESTRING_DOM_VALUE(NAME, #NAME);

/* lwc_string strings */
CORESTRING_LWC_STRING(a);
CORESTRING_LWC_STRING(about);
CORESTRING_LWC_STRING(abscenter);
CORESTRING_LWC_STRING(absmiddle);
CORESTRING_LWC_STRING(align);
CORESTRING_LWC_STRING(applet);
CORESTRING_LWC_STRING(base);
CORESTRING_LWC_STRING(baseline);
CORESTRING_LWC_STRING(body);
CORESTRING_LWC_STRING(bottom);
CORESTRING_LWC_STRING(button);
CORESTRING_LWC_STRING(caption);
CORESTRING_LWC_STRING(charset);
CORESTRING_LWC_STRING(center);
CORESTRING_LWC_STRING(checkbox);
CORESTRING_LWC_STRING(circle);
CORESTRING_LWC_STRING(col);
CORESTRING_LWC_STRING(data);
CORESTRING_LWC_STRING(default);
CORESTRING_LWC_STRING(div);
CORESTRING_LWC_STRING(embed);
CORESTRING_LWC_STRING(file);
CORESTRING_LWC_STRING(filename);
CORESTRING_LWC_STRING(font);
CORESTRING_LWC_STRING(frame);
CORESTRING_LWC_STRING(frameset);
CORESTRING_LWC_STRING(ftp);
CORESTRING_LWC_STRING(h1);
CORESTRING_LWC_STRING(h2);
CORESTRING_LWC_STRING(h3);
CORESTRING_LWC_STRING(h4);
CORESTRING_LWC_STRING(h5);
CORESTRING_LWC_STRING(h6);
CORESTRING_LWC_STRING(head);
CORESTRING_LWC_STRING(hidden);
CORESTRING_LWC_STRING(hr);
CORESTRING_LWC_STRING(html);
CORESTRING_LWC_STRING(http);
CORESTRING_LWC_STRING(https);
CORESTRING_LWC_STRING(icon);
CORESTRING_LWC_STRING(iframe);
CORESTRING_LWC_STRING(image);
CORESTRING_LWC_STRING(img);
CORESTRING_LWC_STRING(includesubdomains);
CORESTRING_LWC_STRING(input);
CORESTRING_LWC_STRING(javascript);
CORESTRING_LWC_STRING(justify);
CORESTRING_LWC_STRING(left);
CORESTRING_LWC_STRING(li);
CORESTRING_LWC_STRING(link);
CORESTRING_LWC_STRING(mailto);
CORESTRING_LWC_STRING(meta);
CORESTRING_LWC_STRING(middle);
CORESTRING_LWC_STRING(no);
CORESTRING_LWC_STRING(noscript);
CORESTRING_LWC_STRING(object);
CORESTRING_LWC_STRING(optgroup);
CORESTRING_LWC_STRING(option);
CORESTRING_LWC_STRING(p);
CORESTRING_LWC_STRING(param);
CORESTRING_LWC_STRING(password);
CORESTRING_LWC_STRING(poly);
CORESTRING_LWC_STRING(polygon);
CORESTRING_LWC_STRING(post);
CORESTRING_LWC_STRING(radio);
CORESTRING_LWC_STRING(rect);
CORESTRING_LWC_STRING(rectangle);
CORESTRING_LWC_STRING(refresh);
CORESTRING_LWC_STRING(reset);
CORESTRING_LWC_STRING(resource);
CORESTRING_LWC_STRING(right);
CORESTRING_LWC_STRING(search);
CORESTRING_LWC_STRING(select);
CORESTRING_LWC_STRING(src);
CORESTRING_LWC_STRING(style);
CORESTRING_LWC_STRING(submit);
CORESTRING_LWC_STRING(table);
CORESTRING_LWC_STRING(tbody);
CORESTRING_LWC_STRING(td);
CORESTRING_LWC_STRING(text);
CORESTRING_LWC_STRING(textarea);
CORESTRING_LWC_STRING(texttop);
CORESTRING_LWC_STRING(tfoot);
CORESTRING_LWC_STRING(th);
CORESTRING_LWC_STRING(thead);
CORESTRING_LWC_STRING(title);
CORESTRING_LWC_STRING(top);
CORESTRING_LWC_STRING(tr);
CORESTRING_LWC_STRING(ul);
CORESTRING_LWC_STRING(url);
CORESTRING_LWC_STRING(yes);
CORESTRING_LWC_STRING(_blank);
CORESTRING_LWC_STRING(_parent);
CORESTRING_LWC_STRING(_self);
CORESTRING_LWC_STRING(_top);
CORESTRING_LWC_STRING(443);

/* unusual lwc strings */
CORESTRING_LWC_VALUE(shortcut_icon, "shortcut icon");
CORESTRING_LWC_VALUE(slash_, "/");
CORESTRING_LWC_VALUE(max_age, "max-age");
CORESTRING_LWC_VALUE(no_cache, "no-cache");
CORESTRING_LWC_VALUE(no_store, "no-store");
CORESTRING_LWC_VALUE(query_auth, "query/auth");
CORESTRING_LWC_VALUE(query_ssl, "query/ssl");
CORESTRING_LWC_VALUE(query_timeout, "query/timeout");
CORESTRING_LWC_VALUE(query_fetcherror, "query/fetcherror");
CORESTRING_LWC_VALUE(x_ns_css, "x-ns-css");

/* mime types */
CORESTRING_LWC_VALUE(multipart_form_data, "multipart/form-data");
CORESTRING_LWC_VALUE(text_css, "text/css");
CORESTRING_LWC_VALUE(unknown_unknown, "unknown/unknown");
CORESTRING_LWC_VALUE(application_unknown, "application/unknown");
CORESTRING_LWC_VALUE(any, "*/*");
CORESTRING_LWC_VALUE(text_xml, "text/xml");
CORESTRING_LWC_VALUE(application_xml, "application/xml");
CORESTRING_LWC_VALUE(text_html, "text/html");
CORESTRING_LWC_VALUE(text_plain, "text/plain");
CORESTRING_LWC_VALUE(application_octet_stream, "application/octet-stream");
CORESTRING_LWC_VALUE(image_gif, "image/gif");
CORESTRING_LWC_VALUE(image_png, "image/png");
CORESTRING_LWC_VALUE(image_jpeg, "image/jpeg");
CORESTRING_LWC_VALUE(image_bmp, "image/bmp");
CORESTRING_LWC_VALUE(image_vnd_microsoft_icon, "image/vnd.microsoft.icon");
CORESTRING_LWC_VALUE(image_webp, "image/webp");
CORESTRING_LWC_VALUE(application_rss_xml, "application/rss+xml");
CORESTRING_LWC_VALUE(application_atom_xml, "application/atom+xml");
CORESTRING_LWC_VALUE(audio_wave, "audio/wave");
CORESTRING_LWC_VALUE(application_ogg, "application/ogg");
CORESTRING_LWC_VALUE(video_webm, "video/webm");
CORESTRING_LWC_VALUE(application_x_rar_compressed, "application/x-rar-compressed");
CORESTRING_LWC_VALUE(application_zip, "application/zip");
CORESTRING_LWC_VALUE(application_x_gzip, "application/x-gzip");
CORESTRING_LWC_VALUE(application_postscript, "application/postscript");
CORESTRING_LWC_VALUE(application_pdf, "application/pdf");
CORESTRING_LWC_VALUE(video_mp4, "video/mp4");
CORESTRING_LWC_VALUE(image_svg, "image/svg+xml");


/* DOM strings */
CORESTRING_DOM_STRING(a);
CORESTRING_DOM_STRING(abort);
CORESTRING_DOM_STRING(afterprint);
CORESTRING_DOM_STRING(align);
CORESTRING_DOM_STRING(alt);
CORESTRING_DOM_STRING(area);
CORESTRING_DOM_STRING(ArrowDown);
CORESTRING_DOM_STRING(ArrowLeft);
CORESTRING_DOM_STRING(ArrowRight);
CORESTRING_DOM_STRING(ArrowUp);
CORESTRING_DOM_STRING(async);
CORESTRING_DOM_STRING(background);
CORESTRING_DOM_STRING(beforeprint);
CORESTRING_DOM_STRING(beforeunload);
CORESTRING_DOM_STRING(bgcolor);
CORESTRING_DOM_STRING(blur);
CORESTRING_DOM_STRING(border);
CORESTRING_DOM_STRING(bordercolor);
CORESTRING_DOM_STRING(cancel);
CORESTRING_DOM_STRING(canplay);
CORESTRING_DOM_STRING(canplaythrough);
CORESTRING_DOM_STRING(cellpadding);
CORESTRING_DOM_STRING(cellspacing);
CORESTRING_DOM_STRING(change);
CORESTRING_DOM_STRING(charset);
CORESTRING_DOM_STRING(class);
CORESTRING_DOM_STRING(classid);
CORESTRING_DOM_STRING(click);
CORESTRING_DOM_STRING(close);
CORESTRING_DOM_STRING(codebase);
CORESTRING_DOM_STRING(color);
CORESTRING_DOM_STRING(cols);
CORESTRING_DOM_STRING(colspan);
CORESTRING_DOM_STRING(content);
CORESTRING_DOM_STRING(contextmenu);
CORESTRING_DOM_STRING(coords);
CORESTRING_DOM_STRING(cuechange);
CORESTRING_DOM_STRING(data);
CORESTRING_DOM_STRING(dblclick);
CORESTRING_DOM_STRING(defer);
CORESTRING_DOM_STRING(DOMAttrModified);
CORESTRING_DOM_STRING(DOMNodeInserted);
CORESTRING_DOM_STRING(DOMNodeInsertedIntoDocument);
CORESTRING_DOM_STRING(DOMSubtreeModified);
CORESTRING_DOM_STRING(drag);
CORESTRING_DOM_STRING(dragend);
CORESTRING_DOM_STRING(dragenter);
CORESTRING_DOM_STRING(dragleave);
CORESTRING_DOM_STRING(dragover);
CORESTRING_DOM_STRING(dragstart);
CORESTRING_DOM_STRING(drop);
CORESTRING_DOM_STRING(durationchange);
CORESTRING_DOM_STRING(emptied);
CORESTRING_DOM_STRING(End);
CORESTRING_DOM_STRING(ended);
CORESTRING_DOM_STRING(error);
CORESTRING_DOM_STRING(Escape);
CORESTRING_DOM_STRING(focus);
CORESTRING_DOM_STRING(frameborder);
CORESTRING_DOM_STRING(hashchange);
CORESTRING_DOM_STRING(height);
CORESTRING_DOM_STRING(Home);
CORESTRING_DOM_STRING(href);
CORESTRING_DOM_STRING(hreflang);
CORESTRING_DOM_STRING(hspace);
/* http-equiv: see below */
CORESTRING_DOM_STRING(id);
CORESTRING_DOM_STRING(input);
CORESTRING_DOM_STRING(invalid);
CORESTRING_DOM_STRING(keydown);
CORESTRING_DOM_STRING(keypress);
CORESTRING_DOM_STRING(keyup);
CORESTRING_DOM_STRING(link);
CORESTRING_DOM_STRING(languagechange);
CORESTRING_DOM_STRING(load);
CORESTRING_DOM_STRING(loadeddata);
CORESTRING_DOM_STRING(loadedmetadata);
CORESTRING_DOM_STRING(loadstart);
CORESTRING_DOM_STRING(map);
CORESTRING_DOM_STRING(marginheight);
CORESTRING_DOM_STRING(marginwidth);
CORESTRING_DOM_STRING(media);
CORESTRING_DOM_STRING(message);
CORESTRING_DOM_STRING(mousedown);
CORESTRING_DOM_STRING(mousemove);
CORESTRING_DOM_STRING(mouseout);
CORESTRING_DOM_STRING(mouseover);
CORESTRING_DOM_STRING(mouseup);
CORESTRING_DOM_STRING(mousewheel);
CORESTRING_DOM_STRING(name);
CORESTRING_DOM_STRING(nohref);
CORESTRING_DOM_STRING(noresize);
CORESTRING_DOM_STRING(nowrap);
CORESTRING_DOM_STRING(offline);
CORESTRING_DOM_STRING(online);
CORESTRING_DOM_STRING(PageDown);
CORESTRING_DOM_STRING(pagehide);
CORESTRING_DOM_STRING(pageshow);
CORESTRING_DOM_STRING(PageUp);
CORESTRING_DOM_STRING(pause);
CORESTRING_DOM_STRING(play);
CORESTRING_DOM_STRING(playing);
CORESTRING_DOM_STRING(popstate);
CORESTRING_DOM_STRING(progress);
CORESTRING_DOM_STRING(ratechange);
CORESTRING_DOM_STRING(readystatechange);
CORESTRING_DOM_STRING(rect);
CORESTRING_DOM_STRING(rel);
CORESTRING_DOM_STRING(reset);
CORESTRING_DOM_STRING(resize);
CORESTRING_DOM_STRING(rows);
CORESTRING_DOM_STRING(rowspan);
CORESTRING_DOM_STRING(scroll);
CORESTRING_DOM_STRING(scrolling);
CORESTRING_DOM_STRING(seeked);
CORESTRING_DOM_STRING(seeking);
CORESTRING_DOM_STRING(select);
CORESTRING_DOM_STRING(selected);
CORESTRING_DOM_STRING(shape);
CORESTRING_DOM_STRING(show);
CORESTRING_DOM_STRING(size);
CORESTRING_DOM_STRING(sizes);
CORESTRING_DOM_STRING(src);
CORESTRING_DOM_STRING(stalled);
CORESTRING_DOM_STRING(storage);
CORESTRING_DOM_STRING(style);
CORESTRING_DOM_STRING(submit);
CORESTRING_DOM_STRING(suspend);
CORESTRING_DOM_STRING(target);
CORESTRING_DOM_STRING(text);
CORESTRING_DOM_STRING(timeupdate);
CORESTRING_DOM_STRING(title);
CORESTRING_DOM_STRING(type);
CORESTRING_DOM_STRING(unload);
CORESTRING_DOM_STRING(valign);
CORESTRING_DOM_STRING(value);
CORESTRING_DOM_STRING(vlink);
CORESTRING_DOM_STRING(volumechange);
CORESTRING_DOM_STRING(vspace);
CORESTRING_DOM_STRING(waiting);
CORESTRING_DOM_STRING(width);
/* DOM node names, not really CSS */
CORESTRING_DOM_STRING(BUTTON);
CORESTRING_DOM_STRING(HTML);
CORESTRING_DOM_STRING(INPUT);
CORESTRING_DOM_STRING(SELECT);
CORESTRING_DOM_STRING(TEXTAREA);
CORESTRING_DOM_STRING(TITLE);
CORESTRING_DOM_STRING(BODY);
CORESTRING_DOM_STRING(HEAD);
CORESTRING_DOM_STRING(SCRIPT);
/* DOM input types, not really CSS */
CORESTRING_DOM_STRING(button);
CORESTRING_DOM_STRING(image);
CORESTRING_DOM_STRING(radio);
CORESTRING_DOM_STRING(checkbox);
CORESTRING_DOM_STRING(file);
/* DOM event prefix */
CORESTRING_DOM_STRING(on);
/* DOM events forwarded from body to window */
CORESTRING_DOM_STRING(onblur);
CORESTRING_DOM_STRING(onerror);
CORESTRING_DOM_STRING(onfocus);
CORESTRING_DOM_STRING(onload);
CORESTRING_DOM_STRING(onresize);
CORESTRING_DOM_STRING(onscroll);
/* Corestrings used by DOM event registration */
CORESTRING_DOM_STRING(autocomplete);
CORESTRING_DOM_STRING(autocompleteerror);
CORESTRING_DOM_STRING(dragexit);
CORESTRING_DOM_STRING(mouseenter);
CORESTRING_DOM_STRING(mouseleave);
CORESTRING_DOM_STRING(wheel);
CORESTRING_DOM_STRING(sort);
CORESTRING_DOM_STRING(toggle);
/* DOM userdata keys, not really CSS */
CORESTRING_DOM_STRING(__ns_key_box_node_data);
CORESTRING_DOM_STRING(__ns_key_libcss_node_data);
CORESTRING_DOM_STRING(__ns_key_file_name_node_data);
CORESTRING_DOM_STRING(__ns_key_image_coords_node_data);
CORESTRING_DOM_STRING(__ns_key_html_content_data);
CORESTRING_DOM_STRING(__ns_key_canvas_node_data);

/* unusual DOM strings */
CORESTRING_DOM_VALUE(text_javascript, "text/javascript");
CORESTRING_DOM_VALUE(http_equiv, "http-equiv");
CORESTRING_DOM_VALUE(html_namespace, "http://www.w3.org/1999/xhtml");

//CORESTRING_NSURL(about_blank, "about:blank");
//CORESTRING_NSURL(about_query_ssl, "about:query/ssl");
//CORESTRING_NSURL(about_query_auth, "about:query/auth");
//CORESTRING_NSURL(about_query_timeout, "about:query/timeout");
//CORESTRING_NSURL(about_query_fetcherror, "about:query/fetcherror");

#undef CORESTRING_LWC_STRING
#undef CORESTRING_DOM_STRING
