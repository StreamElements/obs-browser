/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_task.h>
#include <include/cef_client.h>
#include <include/cef_parser.h>
#include <include/cef_scheme.h>
#include <include/cef_version.h>
#include <include/cef_render_process_handler.h>
#include <include/cef_request_context_handler.h>

#if CHROME_VERSION_BUILD < 3770
#define ENABLE_DECRYPT_COOKIES 1
#else
#define ENABLE_DECRYPT_COOKIES 0
#endif

#if CHROME_VERSION_BUILD < 3770
	#define ENABLE_WASHIDDEN 1
	#if CHROME_VERSION_BUILD >= 3729
		/* When this hack is enabled, the browser source will
		 * perform several additional steps in addition to
		 * calling WasHidden(false):
		 *
		 * Step 1:
		 * Call WasResized(), return (width,height+1) in
		 * GetClientRect() and advance to step 2.
		 *
		 * Step 2:
		 * Call WasHidden(false), WasHidden(true),
		 * WasHidden(false), indicate that GetClientRect()
		 * should return normal size (width,height) and
		 * call WasResized() again.
		 *
		 * All of this is necessary to override a Hardware
		 * Accelerated rendering bug in CEF 3729 where CEF
		 * would stop sending frames in cases where (mostly)
		 * there are 5+ browser sources after calling
		 * WasHidden(true) and WasHidden(false) again.
		 *
		 * Other approaches to overriding this bug (simulating
		 * the Page Visibility API and avoiding calling
		 * WasHidden() at all yielded sub-par performance:
		 * browser sources which are not supposed to be visible
		 * were still decoding and rendering video in the
		 * background, consuming CPU & GPU resources.
		 *
		 * The current known issue with this is that web
		 * pages which use the Page Visibility API, will get
		 * multiple events when the browser source becomes
		 * visible (i.e. instead of "shown", the web page will
		 * have the following sequence of events: "shown",
		 * "hidden", "shown").
		 *
		 * For this hack to work, one must build CEF with the
		 * following patches:
		 *
		 * Fix gclient_util after depot_tools changes (fixes issue #2736)
		 * https://bitbucket.org/chromiumembedded/cef/commits/16a67c450724f60708b8b6af32bd7d547392c485
		 *
		 * Fix OSR use_external_begin_frame support and update VSync setters (fixes issue #2618):
		 * https://bitbucket.org/chromiumembedded/cef/commits/5623338662e9afe558c12f3e4439a3c8da701f1b
		 *
		 * Remove CHECK for size change:
		 * https://chromium-review.googlesource.com/c/chromium/src/+/1792459/2/content/browser/renderer_host/render_widget_host_impl.cc
		 *
		 * This was tested only with CEF 3729 release branch.
		 */
		#define ENABLE_WASHIDDEN_RESIZE_HACK 1
	#else
		#define ENABLE_WASHIDDEN_RESIZE_HACK 0
	#endif
#else
#define ENABLE_WASHIDDEN 0
#define ENABLE_WASHIDDEN_RESIZE_HACK 0
#endif

#if CHROME_VERSION_BUILD >= 3770
#define SendBrowserProcessMessage(browser, pid, msg) \
	browser->GetMainFrame()->SendProcessMessage(pid, msg);
#else
#define SendBrowserProcessMessage(browser, pid, msg) \
	browser->SendProcessMessage(pid, msg);
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
