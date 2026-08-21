-- Native titlebars for Floating Mode. Loaded after Omarchy defaults.
hl.config({
  plugin = {
    hyprbars = {
      enabled = false,
      -- Slightly opaque while retaining a hint of the background beneath it.
      bar_color = "rgba(333333c0)",
      bar_height = 30,
      bar_title_enabled = true,
      bar_text_size = 14,
      bar_text_weight = 600,
      bar_text_font = "Sans",
      bar_text_align = "left",
      bar_buttons_alignment = "right",
      bar_part_of_window = true,
      -- Let Hyprland draw one continuous window border around both the
      -- native titlebar decoration and the application surface.
      bar_precedence_over_border = true,
      bar_padding = 8,
      bar_button_padding = 6,
      icon_on_hover = false,
      button_hover_bg_color = "rgba(ffffff24)",
      close_button_hover_bg_color = "rgba(ff4d4d48)",
    },
  },
})

-- Buttons are declared right-to-left: close, then maximize.
hl.plugin.hyprbars.add_button({
  bg_color = "rgba(00000000)",
  fg_color = "rgba(ffffffff)",
  size = 21,
  icon = "X",
  action = [[hyprctl dispatch 'hl.dsp.window.close({})']],
})

-- A maximized floating window fills the work area, so its decorative border
-- adds no useful separation. This dynamic rule restores the normal themed
-- border automatically as soon as the window leaves maximized state.
hl.window_rule({
  name = "floating-mode-maximized-no-border",
  match = {
    float = true,
    fullscreen_state_internal = 1,
  },
  border_size = 0,
})

hl.plugin.hyprbars.add_button({
  bg_color = "rgba(00000000)",
  fg_color = "rgba(ffffffff)",
  size = 21,
  icon = "□",
  action = [[hyprctl dispatch 'hl.dsp.window.fullscreen({ mode = "maximized", action = "toggle" })']],
})
