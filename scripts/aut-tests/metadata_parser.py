# MetaDataParser
# Inspired by: http://nefaria.com/2012/08/simple-configuration-file-parser-python/
class MetadataParser():
 
    def __init__(self, comment_char = '#', option_char = '='):
        self.comment_char = comment_char
        self.option_char = option_char
 
    def parse(self, filename):
        self.options = {}
        config_file = open(filename)
        for line in config_file:
            if self.comment_char in line:
                line, comment = line.split(self.comment_char, 1)
            if self.option_char in line:
                option, value = line.split(self.option_char, 1)
                option = option.strip()
                value = value.strip()
                value = value.strip('"\'')
                value = value.split()[0]
                self.options[option] = value
        config_file.close()
        return self.options
