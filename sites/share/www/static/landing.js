// SitesLanding contains the landing page's data transformations.  Keeping these
// functions free of DOM state makes the rendered view deterministic and testable.
(function () {
  "use strict";

  function filterSites(rows, query) {
    var needle = String(query || "").toLowerCase();
    if (!needle) return rows.slice();
    return rows.filter(function (row) {
      var slug = String(row.slug || "").toLowerCase();
      var position = 0;
      for (var i = 0; i < slug.length && position < needle.length; i++) {
        if (slug[i] === needle[position]) position++;
      }
      return position === needle.length;
    });
  }

  function sortRows(rows, key, dir) {
    var field = key === "name" ? "slug" : key === "createdAt" ? "createdAtSort" : "createdBy";
    var direction = dir === "desc" ? -1 : 1;
    return rows.slice().sort(function (left, right) {
      var a = String(left[field] || "");
      var b = String(right[field] || "");
      var compare = a < b ? -1 : a > b ? 1 : 0;
      if (compare) return compare * direction;
      return String(left.slug || "") < String(right.slug || "") ? -1 : String(left.slug || "") > String(right.slug || "") ? 1 : 0;
    });
  }

  function paginate(rows, page, size) {
    return rows.slice((page - 1) * size, page * size);
  }

  function nextSort(state, key) {
    if (state.sortKey === key) {
      return { sortKey: key, dir: state.dir === "asc" ? "desc" : "asc" };
    }
    return { sortKey: key, dir: "asc" };
  }

  function defaultState() {
    return { query: "", sortKey: "createdAt", dir: "desc", page: 1 };
  }

  function reduce(state, action) {
    switch (action.type) {
      case "setQuery":
        return { query: action.query, sortKey: state.sortKey, dir: state.dir, page: 1 };
      case "setSort": {
        var sort = nextSort(state, action.key);
        return { query: state.query, sortKey: sort.sortKey, dir: sort.dir, page: 1 };
      }
      case "setPage":
        return { query: state.query, sortKey: state.sortKey, dir: state.dir, page: action.page };
      case "clear":
        return defaultState();
      default:
        return state;
    }
  }

  function computeView(rows, state) {
    var inputCount = rows.length;
    var filtered = filterSites(rows, state.query);
    var sorted = sortRows(filtered, state.sortKey, state.dir);
    var pageCount = Math.max(1, Math.ceil(sorted.length / 10));
    var page = Math.max(1, Math.min(state.page, pageCount));
    var visible = paginate(sorted, page, 10);
    var rangeFrom = sorted.length ? (page - 1) * 10 + 1 : 0;
    var rangeTo = sorted.length ? rangeFrom + visible.length - 1 : 0;
    return {
      rows: visible,
      showControls: inputCount > 0,
      empty: inputCount === 0,
      noMatch: inputCount > 0 && sorted.length === 0,
      showPager: sorted.length > 10,
      page: page,
      pageCount: pageCount,
      rangeFrom: rangeFrom,
      rangeTo: rangeTo,
      sortKey: state.sortKey,
      dir: state.dir,
    };
  }

  globalThis.SitesLanding = { filterSites: filterSites, sortRows: sortRows, paginate: paginate, nextSort: nextSort, defaultState: defaultState, reduce: reduce, computeView: computeView };

  function initController() {
    var data = document.querySelector("#sites-data");
    var rows = JSON.parse(data.textContent);
    var state = defaultState();
    var controls = document.querySelector(".controls");
    var pager = document.querySelector(".pager");
    var noMatch = document.querySelector(".no-match");
    var search = document.querySelector("#site-search");
    var clear = document.querySelector("#site-clear");
    var previous = document.querySelector("#pager-prev");
    var next = document.querySelector("#pager-next");
    var label = document.querySelector("#pager-label");
    var body = document.querySelector(".site-table tbody");
    var headers = document.querySelectorAll("th[data-sort-key]");

    function render() {
      var view = computeView(rows, state);
      body.replaceChildren();
      view.rows.forEach(function (row) {
        var tr = document.createElement("tr");
        var slug = document.createElement("td");
        var anchor = document.createElement("a");
        anchor.href = row.url;
        anchor.textContent = row.slug;
        slug.dataset.label = "Slug";
        slug.appendChild(anchor);
        var visibility = document.createElement("td");
        var badge = document.createElement("span");
        visibility.dataset.label = "Visibility";
        badge.className = "visibility";
        badge.textContent = row.public ? "public" : "private";
        visibility.appendChild(badge);
        var creator = document.createElement("td");
        creator.dataset.label = "Creator";
        creator.textContent = row.createdBy;
        var created = document.createElement("td");
        created.dataset.label = "Created";
        created.textContent = row.createdAt;
        var copy = document.createElement("td");
        var button = document.createElement("button");
        var svgNamespace = "http://www.w3.org/2000/svg";
        var icon = document.createElementNS(svgNamespace, "svg");
        var rect = document.createElementNS(svgNamespace, "rect");
        var path = document.createElementNS(svgNamespace, "path");
        var label = document.createElement("span");
        copy.dataset.label = "Copy";
        button.type = "button";
        button.className = "copy-btn js-only";
        button.dataset.url = row.url;
        icon.setAttribute("aria-hidden", "true");
        icon.setAttribute("viewBox", "0 0 24 24");
        rect.setAttribute("x", "9");
        rect.setAttribute("y", "9");
        rect.setAttribute("width", "11");
        rect.setAttribute("height", "11");
        rect.setAttribute("rx", "1");
        path.setAttribute("d", "M15 9V5a1 1 0 0 0-1-1H5a1 1 0 0 0-1 1v9a1 1 0 0 0 1 1h4");
        icon.append(rect, path);
        label.className = "copy-label";
        label.textContent = "Copy";
        button.append(icon, label);
        copy.appendChild(button);
        tr.append(slug, visibility, creator, created, copy);
        body.appendChild(tr);
      });
      label.textContent = "Page " + view.page + " of " + view.pageCount;
      pager.toggleAttribute("hidden", !view.showPager);
      noMatch.toggleAttribute("hidden", !view.noMatch);
      controls.toggleAttribute("hidden", !view.showControls);
      headers.forEach(function (header) {
        header.setAttribute("aria-sort", view.dir === "asc" ? "ascending" : "descending");
        header.toggleAttribute("aria-sort", header.dataset.sortKey === view.sortKey);
      });
    }

    function dispatch(action) {
      state = reduce(state, action);
      render();
    }

    document.documentElement.className = "js";
    render();
    search.addEventListener("input", function () { dispatch({ type: "setQuery", query: search.value }); });
    search.addEventListener("keydown", function (event) {
      if (event.key === "Escape") {
        search.value = "";
        dispatch({ type: "setQuery", query: "" });
      }
    });
    headers.forEach(function (header) {
      header.addEventListener("click", function () { dispatch({ type: "setSort", key: header.dataset.sortKey }); });
    });
    previous.addEventListener("click", function () { dispatch({ type: "setPage", page: state.page - 1 }); });
    next.addEventListener("click", function () { dispatch({ type: "setPage", page: state.page + 1 }); });
    clear.addEventListener("click", function () {
      search.value = "";
      dispatch({ type: "clear" });
    });
    body.addEventListener("click", function (event) {
      var button = event.target.closest(".copy-btn");
      if (!button) return;
      var url = button.dataset.url;
      var copied = function () {
        var copyLabel = button.querySelector(".copy-label");
        copyLabel.textContent = "Copied";
        button.classList.add("is-copied");
        setTimeout(function () {
          copyLabel.textContent = "Copy";
          button.classList.remove("is-copied");
        }, 1600);
      };
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(url).then(copied);
        return;
      }
      var textarea = document.createElement("textarea");
      textarea.value = url;
      textarea.setAttribute("aria-hidden", "true");
      textarea.style.position = "fixed";
      textarea.style.opacity = "0";
      document.body.appendChild(textarea);
      textarea.select();
      document.execCommand("copy");
      textarea.remove();
      copied();
    });
  }
  if (typeof document !== "undefined") {
    document.addEventListener("DOMContentLoaded", function () { initController(); });
  }
}());
