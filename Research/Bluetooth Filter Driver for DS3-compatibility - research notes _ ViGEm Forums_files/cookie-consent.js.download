
(function (factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else if (typeof define === 'function' && define.amd) {
    define(factory);
  }
})(function () {
  function compiled(helpers, context, guard, iter, helper) {
    var __escape = helpers.__escape;
    var value = context;
    return "<div class=\"cookie-consent\">\n    <button class=\"pull-right btn btn-primary\">" + 
      __escape(guard((context != null) ? context['dismiss'] : null)) + 
      "</button>\n    " + 
      __escape(guard((context != null) ? context['message'] : null)) + 
      " <a target=\"_blank\" rel=\"noopener\" href=\"" + 
      __escape(guard((context != null) ? context['link_url'] : null)) + 
      "\">" + 
      __escape(guard((context != null) ? context['link'] : null)) + 
      "</a>\n</div>\n";
  }

  compiled.blocks = {
    
  };

  return compiled;
})
