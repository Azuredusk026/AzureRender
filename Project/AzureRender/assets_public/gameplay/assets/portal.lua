local requested = false

function trigger(other, entered)
    if entered and other == "hero" and not requested then
        requested = true
        self:load_level("assets:/destination.azurelevel")
    end
end
