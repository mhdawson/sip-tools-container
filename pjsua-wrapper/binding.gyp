{
  'targets': [
    {
      'target_name': 'addon',
      'sources': [ 'pjsua-wrapper.c' ],
      'libraries': [
            '<!@(pkg-config --static --cflags --libs libpjproject)'
      ],
    }
  ]
}
